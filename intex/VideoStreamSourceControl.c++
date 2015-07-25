#include <sstream>
#include <stdexcept>
#include <iostream>
#include <atomic>

#include <QDebug>
#include <QFile>
#include <QString>
#include <QTextStream>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdocumentation"
#pragma clang diagnostic ignored "-Wdocumentation-unknown-command"
#pragma clang diagnostic ignored "-Wold-style-cast"
#pragma clang diagnostic ignored "-Wshift-sign-overflow"
#pragma clang diagnostic ignored "-Wcast-align"
#pragma clang diagnostic ignored "-Wreserved-id-macro"
#pragma clang diagnostic ignored "-Wconversion"
#include <gst/video/video.h>
#pragma clang diagnostic pop

#include "VideoStreamSourceControl.h"

static constexpr char encoderName[] = "encoder";
static constexpr char sinkName[] = "udpsink";

struct debug_tag {};

static QString make_downlink(const QString &host, const QString &port) {
  QString buf;
  QTextStream downlink(&buf);
  downlink << " ! queue ! omxh264dec "
           << " ! videoscale ! video/x-raw,width=720,height=480,framerate=30/1"
           << " ! omxh264enc name=" << encoderName
           << " target_bitrate=400000 control-rate=variable inline-header=true"
           << " periodicty-idr=10 interval-intraframes=10"
           << " ! video/x-h264, profile=(string)high, level=(string)4"
           << " ! rtph264pay config-interval=1 ! queue"
           << " ! udpsink host=" << host << " port=" << port
           << " sync=false name=" << sinkName;
  return buf;
}

static QString make_downlink(const QString &host, const QString &port,
                             debug_tag) {
  QString buf;
  QTextStream downlink(&buf);
  downlink << " ! queue ! videoscale"
           << " ! video/x-raw,width=720,height=480,framerate=30/1"
           << " ! rtpvrawpay ! queue"
           << " ! udpsink host=" << host << " port=" << port
           << " sync=false name=" << sinkName;
  return buf;
}

#define check_nonnull(x) check_nonnull_impl(x, __LINE__, __func__)

template <typename T>
decltype(auto) check_nonnull_impl(T &&t, int lineno, const char *func) {
  if (!t) {
    std::ostringstream os;
    os << func << "(" << lineno << "): "
       << "Null pointer.";
    throw std::runtime_error(os.str());
  }

  return std::forward<T>(t);
}

static QString make_sinkbin(const int replica) {
  QString buf;
  QTextStream filesink(&buf);
  filesink << " ! queue ! output-selector name=s" << replica
           << " pad-negotiation-mode=active";
  filesink << " s" << replica << ". ! fakesink async=0 name=c" << replica;

  return buf;
}

static QGst::PipelinePtr make_pipeline(const int dev, const QString &host,
                                       const QString &port, const bool debug) {
  QString teename("h264");
  QString buf;
  QTextStream pipeline(&buf);

  if (!debug) {
    pipeline << "uvch264src name=cam" << dev << " device=/dev/video" << dev
             << " initial-bitrate=5000000 peak-bitrate=5000000 "
                "average-bitrate=3000000"
             << " mode=mode-video rate-control=vbr auto-start=true"
             << " iframe-period=1 cam0.vidsrc ! h264parse";
  } else {
    pipeline << "videotestsrc name=cam" << dev;
  }

  pipeline << " ! queue ! tee name=" << teename << " " << teename << ".";

  pipeline << (debug ? make_downlink(host, port, debug_tag{})
                     : make_downlink(host, port));

  if (!debug) {
    try {
      QString fsink = make_sinkbin(0);
      pipeline << " " << teename << "." << fsink;
    } catch (const std::exception &e) {
      qDebug() << "Skipping multifilesink: "
               << QString::fromStdString(e.what());
    }

    try {
      QString fsink = make_sinkbin(1);
      pipeline << " " << teename << "." << fsink;
    } catch (const std::exception &e) {
      qDebug() << "Skipping multifilesink: "
               << QString::fromStdString(e.what());
    }
  }

  qDebug() << buf;

  return QGst::Parse::launch(buf.toLatin1().data())
      .dynamicCast<QGst::Pipeline>();
}

/* Encapsulates a single filesink with parser and muxer, supporting start and
 * stop operations.
 */
class MultiFileSink {
  QGst::BinPtr bin;
  QGst::ElementPtr parser;
  QGst::ElementPtr muxer;
  QGst::ElementPtr filesink;

public:
  MultiFileSink()
      : bin(check_nonnull(QGst::Bin::create())),
        parser(check_nonnull(QGst::ElementFactory::make("h264parse"))),
        muxer(check_nonnull(QGst::ElementFactory::make("matroskamux"))),
        filesink(check_nonnull(QGst::ElementFactory::make("filesink"))) {
    filesink->setProperty("async", 0);
    bin->setStateLocked(true);
    bin->add(parser, muxer, filesink);
    bin->linkMany(parser, muxer, filesink);
    auto parserSink = parser->getStaticPad("sink");
    bin->addPad(QGst::GhostPad::create(parserSink, "sink"));
  }

  /* no data may flow in when calling this */
  void start(const QString &fname) {
    qDebug() << "Starting new file" << fname << "on" << bin->name();
    filesink->setProperty("location", fname);
    bin->setStateLocked(false);
    if (!bin->syncStateWithParent())
      throw std::runtime_error("Could not sync state with parent");
  }

  void stop() {
    qDebug() << "Stopping file on" << bin->name();
    auto binSinkPad = check_nonnull(bin->getStaticPad("sink"));
    auto eos = QGst::EosEvent::create();
    binSinkPad->sendEvent(eos);
    bin->setStateLocked(true);
    bin->setState(QGst::StateNull);
  }

  explicit operator QGst::ElementPtr() { return bin.staticCast<QGst::Element>(); }
  explicit operator const QGst::ElementPtr() const {
    return bin.staticCast<QGst::Element>();
  }
  QGst::PadPtr sinkPad() { return check_nonnull(bin->getStaticPad("sink")); }
};

static GstPadProbeReturn iFrameProbe(GstPad *pad, GstPadProbeInfo *info,
                                     gpointer user_data);

/* Encapsulates two MultiFileSinks with an output-selector element in front of
 * them, to change files dynamically, without halting the pipeline.
 */
class StreamFileSink {
  QGst::ElementPtr queue;
  QGst::ElementPtr selector;
  QGst::ElementPtr fakesink;
  std::function<QString(void)> storageLocation;

  std::array<QGst::PadPtr, 3> pads;
  std::array<MultiFileSink, 2> sinks;
  size_t current = 0;
  std::atomic_bool switching{false};

  friend GstPadProbeReturn iFrameProbe(GstPad *pad, GstPadProbeInfo *info,
                                       gpointer user_data);

  QGst::PadPtr dummyPad() { return pads.at(3); }
  decltype(auto) nextIdx() { return (current + 1) % sinks.size(); }

  void doSwitch() {
    using std::swap;

    stop();
    selector->setProperty("active-pad", pads.at(current));

    current = nextIdx();
#if 0
    currentSinkPad->peer()->sendEvent(
        QGst::CapsEvent::create(nextSinkPad->currentCaps()));
#endif
    switching = false;
  }

public:
  StreamFileSink(unsigned replica, const enum intex::Subsystem subsystem,
                 QGst::PipelinePtr pipeline)
      : queue(check_nonnull(QGst::ElementFactory::make("queue"))),
        selector(check_nonnull(QGst::ElementFactory::make("output-selector"))),
        fakesink(check_nonnull(QGst::ElementFactory::make("fakesink"))),
        storageLocation([replica, subsystem] {
          return intex::storageLocation(replica, subsystem);
        }) {
    selector->setProperty("pad-negotiation-mode", "active");
    fakesink->setProperty("async", 0);
    pipeline->add(queue, selector, fakesink);
    pipeline->linkMany(queue, selector, fakesink);
    for (const auto &sink : sinks) {
      pipeline->add(static_cast<QGst::ElementPtr>(sink));
      selector->link(static_cast<QGst::ElementPtr>(sink));
    }
    std::transform(std::begin(sinks), std::end(sinks), std::begin(pads),
                   [](const auto &sink) {
                     return check_nonnull(static_cast<QGst::ElementPtr>(sink)
                                              ->getStaticPad("sink"))
                         ->peer();
                   });
  }

  void start() {
    sinks.at(current).start(storageLocation());
    selector->setProperty("active-pad", pads.at(current));
  }

  void stop() {
    selector->setProperty("active-pad", dummyPad());
    sinks.at(current).stop();
  }

  /* TODO: add future */
  void next() {
    if (switching) {
      return;
    }
    sinks.at(nextIdx()).start(storageLocation());
    switching = true;
    gst_pad_add_probe(selector->getStaticPad("sink"), GST_PAD_PROBE_TYPE_BUFFER,
                      iFrameProbe, this, nullptr);
  }
};

static GstPadProbeReturn iFrameProbe(GstPad *pad, GstPadProbeInfo *info,
                                     gpointer user_data) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wold-style-cast"
  auto buffer = GST_PAD_PROBE_INFO_BUFFER(info);
  if (!GST_BUFFER_FLAG_IS_SET(buffer, GST_BUFFER_FLAG_DELTA_UNIT)) {
#pragma clang diagnostic pop
    static_cast<StreamFileSink *>(user_data)->doSwitch();
    return GST_PAD_PROBE_REMOVE;
  }
  return GST_PAD_PROBE_PASS;
}

/* Manages a single camera with its two replicated streams */
struct VideoStreamSourceControl::Impl {
  QGst::PipelinePtr pipeline;
  StreamFileSink replica0;
  StreamFileSink replica1;

  Impl(const enum intex::Subsystem subsystem, const QString &host,
       const QString &port, unsigned bitrate, const bool debug)
      : pipeline(make_pipeline(0, host, port, debug)),
        replica0(0, subsystem, pipeline), replica1(1, subsystem, pipeline) {
    if (subsystem != intex::Subsystem::Video0 &&
        subsystem != intex::Subsystem::Video1) {
      throw std::runtime_error(
          "FileSinkManager requires subsystem to be Video0 or Video1");
    }
    pipeline->setState(QGst::StatePlaying);
  }
  ~Impl() { pipeline->setState(QGst::StateNull); }

  void next() {
    replica0.next();
    replica1.next();
  }

  void stop() {
    replica0.stop();
    replica1.stop();
  }

  void start() {
    replica0.start();
    replica1.start();
  }
};

VideoStreamSourceControl::VideoStreamSourceControl(
    const enum intex::Subsystem subsystem, const QString &host,
    const QString &port, unsigned bitrate, bool debug)
    : d(std::make_unique<Impl>(subsystem, host, port, bitrate, debug)) {}

VideoStreamSourceControl::~VideoStreamSourceControl() = default;

QGst::ElementPtr VideoStreamSourceControl::getElementByName(const char *name) {
  auto elem = d->pipeline->getElementByName(name);

  if (!elem) {
    std::ostringstream os;
    os << "Pipeline " << d->pipeline->name().toStdString() << " element '"
       << name << "' not found.";
    throw std::runtime_error(os.str());
  }

  return elem;
}

void VideoStreamSourceControl::setBitrate(const uint64_t bitrate) {
  std::cout << "Setting bitrate: " << bitrate << std::endl;
  getElementByName(encoderName)->setProperty("target-bitrate", bitrate);
}

void VideoStreamSourceControl::setPort(const uint16_t port) {
  std::cout << "Setting port: " << port << std::endl;
  getElementByName(sinkName)->setProperty("port", static_cast<gint>(port));
}
