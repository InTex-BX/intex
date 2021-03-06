#include <sstream>
#include <stdexcept>
#include <atomic>
#include <utility>
#include <mutex>

#include "VideoStreamControl.h"

class SinkSwitcher {
  QGst::BinPtr lhs_;
  QGst::BinPtr rhs_;

  std::string lname;
  std::string rname;

  QGst::ElementPtr lhsElem;
  QGst::ElementPtr rhsElem;

  std::pair<GstPad *, gulong> firstProbe;
  std::mutex mutex;

  std::atomic_flag switching{false};
  bool first;

  void exchange(std::pair<GstPad *, gulong> probe) {
    std::unique_lock<std::mutex> lock(mutex);
    if (first) {
      first = false;
      firstProbe = probe;
      return;
    }

    auto lhsPrevPad = lhsElem->getStaticPad("sink")->peer();
    auto rhsPrevPad = rhsElem->getStaticPad("sink")->peer();

    if (lhsElem->setState(QGst::State::StateNull) == QGst::StateChangeFailure) {
      qCritical() << "could not change state of" << lhsElem->name();
    } else {
      lhs_->remove(lhsElem);
    }

    if (rhsElem->setState(QGst::State::StateNull) == QGst::StateChangeFailure) {
      qCritical() << "could not change state of" << rhsElem->name();
    } else {
      rhs_->remove(rhsElem);
    }

    lhs_->add(rhsElem);

    lhsPrevPad->link(rhsElem->getStaticPad("sink"));

    if (rhsElem->setState(QGst::State::StatePlaying) ==
        QGst::StateChangeFailure) {
      qCritical() << "could not change state of" << rhsElem->name();
    }

    rhs_->add(lhsElem);

    rhsPrevPad->link(lhsElem->getStaticPad("sink"));

    if (lhsElem->setState(QGst::State::StatePlaying) ==
        QGst::StateChangeFailure) {
      qCritical() << "could not change state of" << lhsElem->name();
    }

    gst_pad_remove_probe(probe.first, probe.second);
    gst_pad_remove_probe(firstProbe.first, firstProbe.second);

    std::swap(lname, rname);

    switching.clear(std::memory_order_release);
  }

public:
  SinkSwitcher(QGst::BinPtr lhs, QGst::BinPtr rhs, std::string lname_,
               std::string rname_)
      : lhs_(lhs), rhs_(rhs), lname(std::move(lname_)),
        rname(std::move(rname_)) {}

  void operator()() {
    if (switching.test_and_set(std::memory_order_acquire))
      return;

    lhsElem = lhs_->getElementByName(lname.c_str());
    rhsElem = rhs_->getElementByName(rname.c_str());

    if (!lhsElem) {
      std::ostringstream os;
      os << "No element with name " << lname << " in "
         << lhs_->name().toStdString();
      throw std::runtime_error(os.str());
    }
    if (!rhsElem) {
      std::ostringstream os;
      os << "No element with name " << rname << " in "
         << lhs_->name().toStdString();
      throw std::runtime_error(os.str());
    }

    first = true;

    auto blockCallBack = [](GstPad *pad, GstPadProbeInfo *info,
                            gpointer user_data) -> GstPadProbeReturn {
      auto this_ = static_cast<SinkSwitcher *>(user_data);
      this_->exchange(std::make_pair(pad, GST_PAD_PROBE_INFO_ID(info)));
      return GST_PAD_PROBE_OK;
    };

    auto blockPad = lhsElem->getStaticPad("sink")->peer();
    if (!blockPad)
      qCritical() << "No peer lhs";
    gst_pad_add_probe(blockPad, GST_PAD_PROBE_TYPE_BLOCK_DOWNSTREAM,
                      blockCallBack, this, nullptr);
    blockPad = rhsElem->getStaticPad("sink")->peer();
    if (!blockPad)
      qCritical() << "No peer rhs";
    gst_pad_add_probe(blockPad, GST_PAD_PROBE_TYPE_BLOCK_DOWNSTREAM,
                      blockCallBack, this, nullptr);
  }
};

static const char caps[] =
    "application/x-rtp, media=(string)video, clock-rate=(int)90000, "
    "encoding-name=(string)RAW, sampling=(string)YCbCr-4:2:0, "
    "depth=(string)8, width=(string)640, height=(string)360, "
    "colorimetry=(string)BT601-5, payload=(int)96, ssrc=(uint)4055103255, "
    "timestamp-offset=(uint)2574552406, seqnum-offset=(uint)23268";

static const char h264caps[] =
    "application/x-rtp, media=(string)video, clock-rate=(int)90000, "
    "encoding-name=(string)H264, packetization-mode=(string)1, "
    "payload=(int)96";

static QGst::PipelinePtr makePipeline(const bool debug, const uint16_t port,
                                      const QString &widgetName,
                                      const QString &windowName,
                                      const QString &loc) {
  QString pipeline;
  QTextStream s(&pipeline);

#ifdef RTPBIN
  s << "rtpbin name=rtpbin";
#endif
  s << " udpsrc port=" << port << " caps=";

  if (debug) {
    s << "\"" << caps << "\" ! queue ! rtpvrawdepay";
  } else {
    s << "\"" << h264caps << "\" ! queue";
#ifdef RTPBIN
    s << " ! rtpbin.recv_rtp_sink_0 rtpbin.";
#endif
    s << " ! rtph264depay ! queue ! tee name=h264" << port;
    s << " ! queue ! h264parse ! avdec_h264";
  }

  s << " ! videoconvert ! tee name=raw";
  s << " raw. ! queue name=prev"
    << " ! qt5glvideosink name=" << widgetName << " force-aspect-ratio=true "
    << " raw. ! queue"
    << " ! qt5glvideosink name=" << windowName << " force-aspect-ratio=true ";
  s << " h264" << port << ". ! queue ! h264parse ! mpegtsmux";
  s << " ! output-selector name=videoselector" << port
    << " pad-negotiation-mode=active";
  s << " ! filesink sync=false async=false location=" << loc;

  qDebug() << pipeline;

  return QGst::Parse::launch(pipeline).dynamicCast<QGst::Pipeline>();
}

static const char opuscaps[] =
    "application/x-rtp,media=audio,clock-rate=48000,"
    "encoding-name=X-GST-OPUS-DRAFT-SPITTKA-00,"
    "sprop-maxcapturerate=24000,sprop-stereo=0,payload=96,encoding-params=2";

static auto make_audio(const uint16_t port, const QString &loc) {
  QString pipeline;
  QTextStream s(&pipeline);

  QString teename = QString("audio%1").arg(port);
#ifdef RTPBIN
  s << " udpsrc port=" << port + 1 << " ! rtpbin.recv_rtcp_sink_0";
  s << " rtpbin.send_rtcp_src_0 ! udpsink port=" << port + 5
    << " sync=false async=false";
#endif
  s << " udpsrc port=" << port + 2 << " caps=" << opuscaps;
#ifdef RTPBIN
  s << " ! rtpbin.recv_rtp_sink_1 rtpbin. ";
#endif
  s << " ! rtpopusdepay ! queue ! tee name=opus" << port;
  s << " opus" << port << ". ! queue ! matroskamux";
  s << " ! filesink sync=false async=false location=" << loc;
  s << " opus" << port << ". ! queue ! opusdec";
  s << " ! queue ! tee name=audio" << port;

#ifdef RTPBIN
  s << " udpsrc port=" << port + 3 << " ! rtpbin.recv_rtcp_sink_1";
  s << " rtpbin.send_rtcp_src_1 ! udpsink port=" << port + 7
    << " sync=false async=false";
#endif

  return pipeline;
}

static auto make_audio_pipeline(const uint16_t port1, const uint16_t port2,
                                const QString &leftLoc,
                                const QString rightLoc) {
  QString pipeline;
  QTextStream s(&pipeline);

  s << make_audio(port1, leftLoc);
  s << make_audio(port2, rightLoc);

  //s << " interleave name=interleave ! osxaudiosink sync=false async=false ";

  //s << " audio" << port1 << ". ! queue ! audioconvert ! interleave.";
  //s << " audio" << port2 << ". ! queue ! audioconvert ! interleave.";
  s << " audio" << port1 << ". ! queue ! audioconvert ! osxaudiosink sync=false async=false";
  s << " audio" << port2 << ". ! queue ! fakesink sync=false async=false";

  qDebug() << pipeline;

  return QGst::Parse::launch(pipeline).dynamicCast<QGst::Pipeline>();
}

VideoStreamControl::VideoStreamControl(
    VideoWidget &leftWidget, VideoWidget &rightWidget,
    QGst::Ui::VideoWidget &leftWindow, QGst::Ui::VideoWidget &rightWindow,
    const QString &leftLocation, const QString &rightLocation,
    const QString &leftAudioLoc, const QString &rightAudioLoc, const bool debug)
    : pipeline0(makePipeline(debug, 5000, "lwidget", "lwindow", leftLocation)),
      pipeline1(makePipeline(debug, 5010, "rwidget", "rwindow", rightLocation)),
      audio(debug
                ? QGst::PipelinePtr{}
                : make_audio_pipeline(5000, 5010, leftAudioLoc, rightAudioLoc)),
      widgetSwitcher(std::make_unique<SinkSwitcher>(pipeline0, pipeline1,
                                                    "lwidget", "rwidget")),
      windowSwitcher(std::make_unique<SinkSwitcher>(pipeline0, pipeline1,
                                                    "lwindow", "rwindow")) {

  leftWindow.setVideoSink(get(Type::Window, Stream::Left));
  leftWidget.setVideoSink(get(Type::Widget, Stream::Left));
  rightWindow.setVideoSink(get(Type::Window, Stream::Right));
  rightWidget.setVideoSink(get(Type::Widget, Stream::Right));

  pipeline0->setState(QGst::StatePlaying);
  pipeline1->setState(QGst::StatePlaying);
  if (audio)
    audio->setState(QGst::StatePlaying);
}

VideoStreamControl::~VideoStreamControl() {
  pipeline0->setState(QGst::StateNull);
  pipeline1->setState(QGst::StateNull);
  if (audio)
    audio->setState(QGst::StateNull);
}

QGst::ElementPtr VideoStreamControl::get(enum Type type, enum Stream side) {
  std::ostringstream os;
  QGst::PipelinePtr pipeline;

  switch (side) {
  case Stream::Left:
    os << "l";
    pipeline = pipeline0;
    break;
  case Stream::Right:
    os << "r";
    pipeline = pipeline1;
    break;
  }

  switch (type) {
  case Type::Widget:
    os << "widget";
    break;
  case Type::Window:
    os << "window";
    break;
  }

  auto sink = pipeline->getElementByName(os.str().c_str());
  if (!sink) {
    std::ostringstream os_;
    os_ << "No element '" << os.str() << "' found.";
    throw std::runtime_error(os_.str());
  }
  return sink;
}

QGst::PipelinePtr VideoStreamControl::get(const enum Stream side) {
  switch (side) {
  case Stream::Left:
    return pipeline0;
  case Stream::Right:
    return pipeline1;
  }
}

void VideoStreamControl::switchWidgets() { (*widgetSwitcher)(); }
void VideoStreamControl::switchWindows() { (*windowSwitcher)(); }

template <typename Func>
static void modify_source(QGst::PipelinePtr pipeline, Func &&modification) {
  auto src = pipeline->getElementByName("udpsrc0");
  auto ret = src->setState(QGst::StateNull);
  if (ret == QGst::StateChangeFailure)
    qCritical() << "Error changing state";
  modification(src);
  ret = src->setState(QGst::StatePlaying);
  if (ret == QGst::StateChangeFailure)
    qCritical() << "Error changing state";
}

void VideoStreamControl::setPort(const enum Stream side, const int port) {
  qDebug() << "Setting port to" << port;
  modify_source(get(side), [port](QGst::ElementPtr src) {
    src->setProperty("port", static_cast<gint>(port));
  });
}

void VideoStreamControl::setAddress(const QString &address) {
  modify_source(get(Stream::Left), [&address](QGst::ElementPtr src) {
    src->setProperty("address", address.toStdString().data());
  });
  modify_source(get(Stream::Right), [&address](QGst::ElementPtr src) {
    src->setProperty("address", address.toStdString().data());
  });
}
