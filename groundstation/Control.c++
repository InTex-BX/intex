#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSpacerItem>
#include <QWidget>
#include <QSizePolicy>
#include <QSlider>
#include <QLabel>
#include <QObject>
#include <QApplication>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QKeySequence>

#include "Control.h"

#include "qgst.h"
#include "asio.h"
#include "VideoWindow.h"
#include "VideoWidget.h"
#include "VideoStreamControl.h"
#include "PipelineModifier.h"

struct Control::Impl {
  VideoWindow leftWindow;
  VideoWindow rightWindow;

  VideoWidget *leftVideoWidget;
  VideoWidget *rightVideoWidget;

  VideoStreamControl videoControl;

  QShortcut switchWidgets_;
  QShortcut switchWindows_;

  Impl(QWidget *parent)
      : leftWindow(parent), rightWindow(parent),
        leftVideoWidget(new VideoWidget), rightVideoWidget(new VideoWidget),
        videoControl(*leftVideoWidget, *rightVideoWidget,
                     *leftWindow.videoWidget(), *rightWindow.videoWidget()),
        switchWidgets_(tr("Ctrl+X"), parent, SLOT(switchWidgets())),
        switchWindows_(tr("Ctrl+Shift+X"), parent, SLOT(switchWindows())) {
    leftWindow.setWindowTitle("InTex Live Feed 0");
    rightWindow.setWindowTitle("InTex Live Feed 1");
  }

  ~Impl() {}
  void switchWidgets() { videoControl.switchWidgets(); }
  void switchWindows() { videoControl.switchWindows(); }
};

Control::Control(QWidget *parent)
    : QMainWindow(parent), d_(std::make_unique<Control::Impl>(this)) {
  setWindowTitle(QCoreApplication::applicationName());

  auto mainMenu = menuBar()->addMenu(tr("Menu"));
  mainMenu->addAction(tr("C&onnect"), this, SLOT(onConnect()),
                      QKeySequence::Open);
  mainMenu->addAction(tr("Disconnect"), this, SLOT(onDisconnect()),
                      QKeySequence::Close);

  mainMenu->addAction(tr("Preferences"));

  auto centralWidget = new QWidget;
  setCentralWidget(centralWidget);

  auto centralLayout = new QVBoxLayout(centralWidget);

  auto videoWidget = new QWidget;
  auto videoLayout = new QHBoxLayout(videoWidget);

  videoLayout->addWidget(d_->leftVideoWidget);
  videoLayout->addWidget(d_->rightVideoWidget);
  connect(d_->leftVideoWidget, &VideoWidget::widgetClicked, &d_->leftWindow,
          &VideoWindow::show);
  connect(d_->rightVideoWidget, &VideoWidget::widgetClicked, &d_->rightWindow,
          &VideoWindow::show);

  centralLayout->addWidget(videoWidget);

  auto controlWidget = new QWidget;
  auto controlLayout = new QHBoxLayout(controlWidget);

  auto bitrateSlider = new QSlider(Qt::Horizontal);
  bitrateSlider->setMinimum(1000);
  bitrateSlider->setMaximum(1000 * 1000);
  bitrateSlider->setSingleStep(1000);
  bitrateSlider->setPageStep(10 * 1000);
  bitrateSlider->setSizePolicy(QSizePolicy::MinimumExpanding,
                               QSizePolicy::Fixed);

  auto bitrateLabel = new QLabel;
  bitrateLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
  connect(bitrateSlider, &QSlider::sliderMoved, bitrateLabel,
          static_cast<void (QLabel::*)(int)>(&QLabel::setNum));
  connect(bitrateSlider, &QSlider::valueChanged, bitrateLabel,
          static_cast<void (QLabel::*)(int)>(&QLabel::setNum));
  bitrateSlider->setValue(400 * 1000);
  bitrateSlider->setTracking(false);

  controlLayout->addWidget(bitrateSlider);
  controlLayout->addWidget(bitrateLabel);

  centralLayout->addWidget(controlWidget);
  centralLayout->addItem(new QSpacerItem(0, 0, QSizePolicy::MinimumExpanding,
                                         QSizePolicy::MinimumExpanding));

#if 0
  QPalette palette;
  palette.setBrush(this->backgroundRole(),
                   QBrush(QImage("backgroundImage.jpg")));

  this->setPalette(palette);
#endif
}

Control::~Control() = default;

void Control::onConnect() {}
void Control::onDisconnect() {}
void Control::switchWidgets() { d_->switchWidgets(); }
void Control::switchWindows() { d_->switchWindows(); }
void Control::onBitrateChanged(int bitrate){}

void onBusMessage(const QGst::MessagePtr &message);

#pragma clang diagnostic ignored "-Wpadded"
#include "moc_Control.cpp"
