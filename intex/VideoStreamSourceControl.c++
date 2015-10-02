#include <sstream>
#include <stdexcept>
#include <iostream>

#include <cstring>

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QObject>
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
#include "sysfs.h"

static constexpr char encoderName[] = "encoder";
static constexpr char sinkName[] = "udpsink";

static QString make_udpsink(const QString &host, const uint16_t port) {
  QString buf;
  QTextStream udpsink(&buf);

  udpsink << " ! queue ! udpsink host=" << host << " port=" << port
          << " sync=false async=false";
  return buf;
}

static QString make_encode() {
  QString buf;
  QTextStream downlink(&buf);
  downlink << " ! omxh264enc name=" << encoderName
           << " target_bitrate=400000 control-rate=variable inline-header=true"
           << " periodicty-idr=250 interval-intraframes=250"
           << " ! video/x-h264,profile=baseline"
           << " ! h264parse"
           << " ! rtph264pay config-interval=1";
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

static auto toVideoDevice(const enum intex::Subsystem subsys) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wswitch-enum"
  switch (subsys) {
  case intex::Subsystem::Video0:
    return findDevice(0);
  case intex::Subsystem::Video1:
    return findDevice(1);
  default:
    throw std::runtime_error("Subsystem not supported.");
  }
#pragma clang diagnostic pop
}

static QGst::PipelinePtr make_pipeline(const enum intex::Subsystem subsys,
                                       const QString &host, const uint16_t port,
                                       const bool debug) {
  const QString devName(/*deviceName(subsys)*/"cam");
  QString buf;
  QTextStream pipeline(&buf);
  QPair<QString, QString> device;
  QString error;

  try {
    device = toVideoDevice(subsys);
  } catch (const std::runtime_error &e) {
    error = e.what();
  }

  bool have_device = !debug && error.isEmpty();
#ifdef RTPBIN
  pipeline << " rtpbin name=rtpbin";
#endif

  if (have_device) {
    /* uvch264src */
    pipeline << " uvch264src name=" << devName;
    pipeline << " num-buffers=-1 device=" << device.first;
    pipeline << " enable-sei=true";
    pipeline << " fixed-framerate=true";
    pipeline << " async-handling=true";
    pipeline << " message-forward=true";
    // pipeline << " auto-start=true";
    pipeline << " initial-bitrate=5000000 peak-bitrate=5000000";
    pipeline << " average-bitrate=3000000 rate-control=vbr";
    pipeline << " mode=mode-video iframe-period=2000 ";

    /* vidsrc */
    pipeline << " " << devName << ".vidsrc ! queue ! "
             << "video/x-h264,width=1280,height=720,stream-format=byte-stream"
             << " ! h264parse ! queue name=videoqueue ";
    pipeline << " ! splitmuxsink name=videomux muxer=mpegtsmux";
    pipeline << " location=/media/usb-raid/video/fallback-video" << port
             << "-%05d.mpeg";
    pipeline << " max-size-time=0 max-size-bytes=0";

    /* vfsrc */
    pipeline << " " << devName << ".vfsrc ! queue ! "
             << "video/x-raw,format=I420,width=640,height=360 ! videoconvert"
             << make_encode();
  } else {
    pipeline << " videotestsrc name=" << devName << " pattern=smpte100";
    pipeline
        << " ! video/x-raw,format=I420,framerate=24/1,width=640,height=360";
    pipeline << " ! textoverlay font-desc=\"Sans 50\" shaded-background=true";
    pipeline << " text=\"";
    if (debug)
      pipeline << "Debug mode";
    else
      pipeline << error;
    pipeline << "\" ! videoconvert name=video" << make_encode();
  }

#ifdef RTPBIN
  pipeline << " ! rtpbin.send_rtp_sink_0 rtpbin.send_rtp_src_0 ! ";
#endif
  pipeline << make_udpsink(host, port);
#ifdef RTPBIN
  pipeline << " rtpbin.send_rtcp_src_0 ! " << make_udpsink(host, port + 1);
  pipeline << " udpsrc port=" << port + 5 << " ! rtpbin.recv_rtcp_sink_0";
#endif

  if (have_device) {
    /* alsasrc */
    pipeline << " alsasrc name=micro device=" << device.second;
    pipeline << " provide-clock=true";
    pipeline << " do-timestamp=true";
    pipeline << " ! audio/x-raw,rate=32000 ! queue ! tee name=camaudio";

    /* recording */
    pipeline << " camaudio. ! queue ! volume volume=2.0"
             << " ! audioconvert ! avenc_ac3 bitrate=128000"
             << " ! queue name=audioqueue";
    pipeline << " ! output-selector name=audio-selector "
                "pad-negotiation-mode=active";
    pipeline << " ! fakesink name=audiofakesink sync=false async=false";
    pipeline << " audio-selector.";
    pipeline << " ! splitmuxsink name=audiomux"
             << " max-size-time=0 max-size-bytes=0"
             << " location=/media/usb-raid/video/fallback-audio" << port
             << "%05d.mp4";

    /* stream */
    pipeline << " camaudio.";
  } else {
    pipeline << " audiotestsrc ! audio/x-raw,rate=32000";
  }

  /* encoder */
  pipeline << " ! queue ! deinterleave ! volume name=volume volume=2.0";
  pipeline << " ! audioresample ! audio/x-raw,rate=8000";
  pipeline << " ! opusenc max-payload-size=500 bitrate=8000"
           << " bandwidth=narrowband";
  pipeline << " ! rtpopuspay"
           << " ! application/x-rtp,encoding-name=X-GST-OPUS-DRAFT-SPITTKA-00";
#ifdef RTPBIN
  pipeline << " ! rtpbin.send_rtp_sink_1 rtpbin.send_rtp_src_1";
#endif
  pipeline << make_udpsink(host, port + 2);
#ifdef RTPBIN
  pipeline << " rtpbin.send_rtcp_src_1 ! " << make_udpsink(host, port + 3);
  pipeline << " udpsrc port=" << port + 7 << " ! rtpbin.recv_rtcp_sink_1";
#endif

  qDebug() << buf;

  return QGst::Parse::launch(buf.toLatin1().data())
      .dynamicCast<QGst::Pipeline>();
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wold-style-cast"
static GstPadProbeReturn fix_buffer_timestamp_probe(GstPad *,
                                                    GstPadProbeInfo *info,
                                                    gpointer user_data) {
  GstBuffer *buffer = GST_PAD_PROBE_INFO_BUFFER(info);

  if (!GST_CLOCK_TIME_IS_VALID(GST_BUFFER_TIMESTAMP(buffer))) {
    auto src = static_cast<GstElement *>(user_data);
    GstClock *clock;
    if (src != nullptr && (clock = GST_ELEMENT_CLOCK(src)) != nullptr) {
      gst_object_ref(clock);
      GST_BUFFER_TIMESTAMP(buffer) = gst_clock_get_time(clock);
      gst_object_unref(clock);
    }
  }

  return GST_PAD_PROBE_OK;
}
#pragma clang diagnostic pop

/* Encapsulates two MultiFileSinks with an output-selector element in front of
 * them, to change files dynamically, without halting the pipeline.
 */
class StreamFileSink : public QObject {
  Q_OBJECT

  std::function<QString(void)> storageLocation;

  QGst::PipelinePtr pipeline;
  QGst::ElementPtr audioselector;
  QGst::ElementPtr audiomux;
  QGst::ElementPtr videomux;
  QGst::ElementPtr videofakesink;
  QGst::ElementPtr audiofakesink;
  QGst::PadPtr audiofakesinkpad;
  QGst::PadPtr audiofilesinkpad;

  const char *videoLocation(const guint &) {
    return strdup(storageLocation().toLocal8Bit().constData());
  }

  const char *audioLocation(const guint &) {
    return strdup(storageLocation().append(".mp4").toLocal8Bit().constData());
  }

public:
  StreamFileSink(const enum intex::Subsystem subsystem,
                 QGst::PipelinePtr pipeline_)
      : storageLocation(
            [subsystem] { return intex::storageLocation(subsystem); }),
        pipeline(pipeline_), audioselector(check_nonnull(
                                 pipeline->getElementByName("audio-selector"))),
        audiomux(check_nonnull(pipeline->getElementByName("audiomux"))),
        videomux(check_nonnull(pipeline->getElementByName("videomux"))),
        videofakesink(pipeline->getElementByName("videofakesink")),
        audiofakesink(pipeline->getElementByName("audiofakesink")),
        audiofakesinkpad(check_nonnull(audioselector->getStaticPad("src_0"))),
        audiofilesinkpad(check_nonnull(audioselector->getStaticPad("src_1"))) {
    auto src = pipeline->getElementByName("cam");
    gst_pad_add_probe(src->getStaticPad("vidsrc"), GST_PAD_PROBE_TYPE_BUFFER,
                      fix_buffer_timestamp_probe,
                      pipeline->getElementByName("micro"), NULL);
    QGlib::connect(videomux, "format-location", this,
                   &StreamFileSink::videoLocation);
    QGlib::connect(audiomux, "format-location", this,
                   &StreamFileSink::audioLocation);
  }

  void start() {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wold-style-cast"
    g_signal_emit_by_name(
        G_OBJECT(static_cast<GstElement *>(pipeline->getElementByName("cam"))),
        "start-capture", NULL);
    audioselector->setProperty("active-pad", audiofilesinkpad);
    qDebug() << "Started";
#pragma clang diagnostic pop
  }

  void stop() {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wold-style-cast"
    g_signal_emit_by_name(
        G_OBJECT(static_cast<GstElement *>(pipeline->getElementByName("cam"))),
        "stop-capture", NULL);
    audioselector->setProperty("active-pad", audiofakesinkpad);
#pragma clang diagnostic pop
  }

  void next() {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wold-style-cast"
    g_signal_emit_by_name(G_OBJECT(static_cast<GstElement *>(videomux)),
                          "next-file", NULL);
    g_signal_emit_by_name(G_OBJECT(static_cast<GstElement *>(audiomux)),
                          "next-file", NULL);
#pragma clang diagnostic pop
  }
};

/* Manages a single camera with its two replicated streams */
struct VideoStreamSourceControl::Impl {
  QGst::PipelinePtr pipeline;
  StreamFileSink filesink;

  Impl(const enum intex::Subsystem subsystem, const QString &host,
       const uint16_t port, unsigned bitrate, const bool debug)
      : pipeline(make_pipeline(subsystem, host, port, debug)),
        filesink(subsystem, pipeline) {
    if (subsystem != intex::Subsystem::Video0 &&
        subsystem != intex::Subsystem::Video1) {
      throw std::runtime_error(
          "FileSinkManager requires subsystem to be Video0 or Video1");
    }
    pipeline->setState(QGst::StatePlaying);
  }
  ~Impl() noexcept { pipeline->setState(QGst::StateNull); }
};

VideoStreamSourceControl::VideoStreamSourceControl(
    const enum intex::Subsystem subsystem, const QString &host,
    const uint16_t port, unsigned bitrate, bool debug)
    : d(std::make_unique<Impl>(subsystem, host, port, bitrate, debug)) {}

VideoStreamSourceControl::~VideoStreamSourceControl() = default;

QGst::ElementPtr VideoStreamSourceControl::getElementByName(const char *name) {
  auto elem = d->pipeline->getElementByName(name);

  if (!elem) {
    std::ostringstream os;
    os << "Pipeline " << d->pipeline->name().toStdString() << " element '"
       << name << "' not found.";
    qDebug() << "Element " << name << " not found";
    throw std::runtime_error(os.str());
  }

  return elem;
}

void VideoStreamSourceControl::setVolume(const float volume) {
  qDebug() << "Setting volume:" << volume;
  getElementByName("volume")->setProperty("volume", volume);
}

void VideoStreamSourceControl::setBitrate(const uint64_t bitrate) {
  std::cout << "Setting bitrate: " << bitrate << std::endl;
  getElementByName(encoderName)->setProperty("target-bitrate", bitrate);
}

void VideoStreamSourceControl::setPort(const uint16_t port) {
  std::cout << "Setting port: " << port << std::endl;
  getElementByName(sinkName)->setProperty("port", static_cast<gint>(port));
}

void VideoStreamSourceControl::start() { d->filesink.start(); }
void VideoStreamSourceControl::stop() { d->filesink.stop(); }
void VideoStreamSourceControl::next() { d->filesink.next(); }

#include "VideoStreamSourceControl.moc"
