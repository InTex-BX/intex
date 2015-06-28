#include <QMouseEvent>
#include <QSize>
#include <QWidget>
#include <QSizePolicy>

#include "VideoWidget.h"
#include "AspectRatioLayout.h"

class VideoWidget::Container : public QWidget {
  QGst::Ui::VideoWidget *widget_;
  QSize size_;
  static constexpr QSize minSize{16, 9};

public:
  explicit Container(QWidget *parent = nullptr)
      : QWidget(parent), widget_(new QGst::Ui::VideoWidget(this)),
        size_(minSize) {
    auto layout = new QVBoxLayout(this);
    layout->addWidget(widget_);
    layout->setSpacing(0);
    layout->setMargin(0);
    layout->setContentsMargins(0, 0, 0, 0);
    QPalette palette = this->palette();
    palette.setColor(backgroundRole(), QColor(0, 0, 255));
    setPalette(palette);
    setAutoFillBackground(true);
    setMinimumSize(minSize);
  }
  ~Container();

  QSize sizeHint() const override { return size_; }
  void setVideoSink(const QGst::ElementPtr &sink_) {
    if (!sink_) {
      size_ = minSize;
    }
    widget_->setVideoSink(sink_);
    if (sink_) {
      /* connection is automatically broken, when either object is destroyed */
      QGlib::connect(widget_->videoSink()->getStaticPad("sink"), "notify::caps",
                     this, &VideoWidget::Container::onVideoResized);
    }
  }
  QGst::ElementPtr videoSink() const { return widget_->videoSink(); }
  void releaseVideoSink() {
    size_ = minSize;
    widget_->releaseVideoSink();
  }

private:
  void onVideoResized(const QGlib::ParamSpecPtr &);
};

VideoWidget::Container::~Container() = default;
constexpr QSize VideoWidget::Container::minSize;

VideoWidget::VideoWidget(QWidget *parent)
    : QFrame(parent), container(new Container(this)) {
  setFrameStyle(QFrame::Box | QFrame::Plain);
  auto layout = new AspectRatioLayout(this);
  layout->addWidget(container);
  layout->setSpacing(0);
  layout->setMargin(0);
  layout->setContentsMargins(0, 0, 0, 0);
  QPalette palette = this->palette();
  palette.setColor(backgroundRole(), QColor(0, 255, 0));
  setPalette(palette);
  setAutoFillBackground(true);
}

void VideoWidget::mouseDoubleClickEvent(QMouseEvent *event) {
  if (event->type() == QEvent::MouseButtonDblClick) {
    Q_EMIT widgetClicked();
  }
}

QGst::ElementPtr VideoWidget::videoSink() const {
  return container->videoSink();
}

void VideoWidget::releaseVideoSink() { container->releaseVideoSink(); }
void VideoWidget::setVideoSink(const QGst::ElementPtr &sink_) {
  container->setVideoSink(sink_);
}

void VideoWidget::Container::onVideoResized(const QGlib::ParamSpecPtr &) {
  auto caps = videoSink()->getStaticPad("sink")->currentCaps();
  bool foundSize = false;
  if (caps) {
    for (auto i = unsigned{0}; i < caps->size(); ++i) {
      auto structure = caps->internalStructure(i);
      if (structure->hasField("width") && structure->hasField("height")) {
        size_.setWidth(structure->value("width").toInt());
        size_.setHeight(structure->value("height").toInt());
        foundSize = true;
      }
    }
  }
  if (!foundSize) {
    size_ = minSize;
  }

  updateGeometry();
}

#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wundefined-reinterpret-cast"
#include "moc_VideoWidget.cpp"
