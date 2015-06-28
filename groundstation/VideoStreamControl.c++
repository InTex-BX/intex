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

static const char test_pipeline[] =
    "videotestsrc ! videoconvert ! tee name=raw "
    "raw. ! queue name=prev ! qt5glvideosink name=lwidget force-aspect-ratio=true "
    "raw. ! queue ! qt5glvideosink name=lwindow force-aspect-ratio=true ";

static const char test_pipeline_[] =
    "videotestsrc ! videoflip method=clockwise ! videoconvert ! tee name=raw "
    "raw. ! queue name=prev ! qt5glvideosink name=rwidget force-aspect-ratio=true "
    "raw. ! queue ! qt5glvideosink name=rwindow force-aspect-ratio=true ";

VideoStreamControl::VideoStreamControl(VideoWidget &leftWidget,
                                       VideoWidget &rightWidget,
                                       QGst::Ui::VideoWidget &leftWindow,
                                       QGst::Ui::VideoWidget &rightWindow)
    : pipeline0(QGst::Parse::launch(QString(test_pipeline))
                    .dynamicCast<QGst::Pipeline>()),
      pipeline1(QGst::Parse::launch(QString(test_pipeline_))
                    .dynamicCast<QGst::Pipeline>()),
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
}

VideoStreamControl::~VideoStreamControl() {
  pipeline0->setState(QGst::StateNull);
  pipeline1->setState(QGst::StateNull);
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
