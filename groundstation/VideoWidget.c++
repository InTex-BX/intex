#include <QMouseEvent>
#include <QSize>
#include <QWidget>
#include <QSizePolicy>

#include "VideoWidget.h"
#include "AspectRatioLayout.h"

class VideoWidget::Container : public QWidget {
  Q_OBJECT

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
    layout->setContentsMargins(0, 0, 0, 0);
    setMinimumSize(minSize);
    connect(this, &VideoWidget::Container::resolutionChanged, this,
            &VideoWidget::Container::updateGeometry,
            Qt::ConnectionType::QueuedConnection);
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

Q_SIGNALS:
  void resolutionChanged();
};

VideoWidget::Container::~Container() = default;
constexpr QSize VideoWidget::Container::minSize;

VideoWidget::VideoWidget(QWidget *parent)
    : QFrame(parent), container(new Container(this)) {
  auto layout = new AspectRatioLayout(this);
  layout->addWidget(container);
  layout->setSpacing(0);
  layout->setContentsMargins(0, 0, 0, 0);
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
  if (caps) {
    for (auto i = unsigned{0}; i < caps->size(); ++i) {
      auto structure = caps->internalStructure(i);
      if (structure->hasField("width") && structure->hasField("height")) {
        size_.setWidth(structure->value("width").toInt());
        size_.setHeight(structure->value("height").toInt());
        qDebug() << "Rescaling to" << size_;
        Q_EMIT resolutionChanged();
        return;
      }
    }
  }
}


#pragma clang diagnostic ignored "-Wundefined-reinterpret-cast"
#include "moc_VideoWidget.cpp"
#include "VideoWidget.moc"
