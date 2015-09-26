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
#include <QPushButton>
#include <QSizePolicy>
#include <QLineEdit>
#include <QIntValidator>
#include <QHostAddress>
#include <QUdpSocket>
#include <QTimer>
#include <QTextStream>
#include <QByteArray>

#include <iostream>
#include <chrono>

#include <kj/debug.h>
#include <kj/array.h>
#include <capnp/serialize.h>
#include <capnp/common.h>
#include <capnp/endian.h>

#include "Control.h"

#include "qgst.h"
#include "asio.h"
#include "VideoWindow.h"
#include "VideoWidget.h"
#include "VideoStreamControl.h"
#include "IntexWidget.h"
#include "IntexRpcClient.h"
#include "intex.h"

static IntexWidget *log_instance = nullptr;
static void output(QtMsgType type, const QMessageLogContext &,
                   const QString &msg) {
  if (log_instance != nullptr) {
    log_instance->log(msg);
  }
  std::cerr << msg.toStdString() << std::endl;
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wweak-vtables"
class QByteArrayMessageReader : public capnp::MessageReader {
  QByteArray &buffer;
  QVector<kj::ArrayPtr<const ::capnp::word>> segments;

public:
  QByteArrayMessageReader(QByteArray &buffer_,
                          capnp::ReaderOptions options = capnp::ReaderOptions())
      : capnp::MessageReader(options), buffer(buffer_) {
    auto buffersize = static_cast<size_t>(buffer.size());

    if (buffersize < sizeof(capnp::word)) {
      /* empty message */
      return;
    }

    const capnp::_::WireValue<uint32_t> *table =
        reinterpret_cast<const capnp::_::WireValue<uint32_t> *>(buffer.data());

    uint segmentCount = table[0].get() + 1;
    size_t offset = segmentCount / 2u + 1u;

    KJ_REQUIRE(buffersize >= offset,
               "Message ends prematurely in segment table.") {
      return;
    }

    for (uint segment = 0; segment < segmentCount; ++segment) {
      uint segmentSize = table[segment + 1].get();

      KJ_REQUIRE(buffersize >= offset + segmentSize,
                 "Message ends prematurely in first segment.") {
        return;
      }

      segments.append(kj::ArrayPtr<const capnp::word>(
          reinterpret_cast<const capnp::word *>(buffer.data()) + offset,
          offset + segmentSize));

      offset += segmentSize;
    }
  }

  kj::ArrayPtr<const capnp::word> getSegment(uint id) override {
    auto id_ = static_cast<int>(id);
    if (id_ < segments.size()) {
      return segments.at(id_);
    } else {
      return nullptr;
    }
  }
};
#pragma clang diagnostic pop

static void gpio_callback(QAction *menuItem, const InTexHW hw,
                          IntexRpcClient &client, const bool on) {
  menuItem->setEnabled(false);
  client.setGPIO(hw, on, [on, menuItem, hw](const bool success) {
    menuItem->setChecked(success ? on : !on);
    qDebug() << "Turning" << hw << (on ? "on" : "off")
             << (success ? "was successful" : "failed");
    menuItem->setEnabled(true);
  });
}

struct Control::Impl {
  VideoWindow leftWindow;
  VideoWindow rightWindow;

  VideoWidget *leftVideoWidget;
  VideoWidget *rightVideoWidget;

  QSlider *bitrateSlider;
  QSlider *splitSlider;

  IntexWidget * intexWidget;

  VideoStreamControl videoControl;

  QShortcut switchWidgets_;
  QShortcut switchWindows_;
  QShortcut showNormal_;

  QWidget *leftVideoControl;
  QWidget *rightVideoControl;

  IntexRpcClient client;

  QUdpSocket telemetry_socket;
  QUdpSocket log_socket;

  void bind_socket(QAbstractSocket *socket, quint16 port, QString what) {
    if (socket->bind(QHostAddress::Any, port)) {
      qDebug().nospace() << "Listening for " << what << " datagrams on "
                         << socket->localAddress() << ":"
                         << socket->localPort();
    } else {
      using namespace std::chrono;
      using namespace std::literals::chrono_literals;
      qDebug() << "Error binding socket to receive " << what
               << " datagrams:" << socket->error() << ". Retrying";
      QTimer::singleShot(
          duration_cast<milliseconds>(5s).count(),
          [this, socket, port, what] { bind_socket(socket, port, what); });
    }
  }

  void handle_log_datagram(QByteArray &buffer) {
    QTextStream is(&buffer);
    QString time;
    QString cat;
    QString msg;
    is >> time >> cat >> msg;
    qDebug() << time << cat << msg;
  }

  void handle_telemetry_datagram(QByteArray &buffer) {
    auto reader = QByteArrayMessageReader(buffer);
    auto telemetry = reader.getRoot<Telemetry>();

    auto cpu_temp = telemetry.getCpuTemperature();

    if (cpu_temp.hasError()) {
      qDebug() << cpu_temp.getError().getReason().cStr();
    } else {
      qDebug() << cpu_temp.getTimestamp() << cpu_temp.getReading().getValue();
    }

    auto vna_temp = telemetry.getVnaTemperature();

    if (vna_temp.hasError()) {
      qDebug() << vna_temp.getError().getReason().cStr();
    } else {
      qDebug() << vna_temp.getTimestamp() << vna_temp.getReading().getValue();
    }
  }

  void handle_datagram(QUdpSocket &socket,
                       void (Control::Impl::*handler)(QByteArray &)) {
    for (; socket.hasPendingDatagrams();) {
      auto size = socket.pendingDatagramSize();
      if (size < 0) {
        qDebug() << "No datagram ready.";
      }

      QByteArray buffer;
      buffer.resize(static_cast<int>(size));
      auto ret = socket.readDatagram(buffer.data(), buffer.size());
      if (ret < 0) {
        qCritical() << "Could not read datagram.";
        return;
      }

      (this->*handler)(buffer);
    }
  }

  Impl(QWidget *parent)
      : leftWindow(parent), rightWindow(parent),
        leftVideoWidget(new VideoWidget), rightVideoWidget(new VideoWidget),
        bitrateSlider(new QSlider(Qt::Horizontal)),
        splitSlider(new QSlider(Qt::Horizontal)), intexWidget(new IntexWidget),
        videoControl(*leftVideoWidget, *rightVideoWidget,
                     *leftWindow.videoWidget(), *rightWindow.videoWidget()),
        switchWidgets_(tr("Ctrl+X"), parent, SLOT(switchWidgets())),
        switchWindows_(tr("Ctrl+Shift+X"), parent, SLOT(switchWindows())),
        showNormal_(tr("Esc"), parent, SLOT(showNormal())), client("*", 1234) {
    qInstallMessageHandler(output);
    log_instance = intexWidget;

    connect(&telemetry_socket, &QAbstractSocket::readyRead, [this] {
      handle_datagram(telemetry_socket,
                      &Control::Impl::handle_telemetry_datagram);
    });
    bind_socket(&telemetry_socket, 54431, "Telemetry");

    connect(&log_socket, &QAbstractSocket::readyRead, [this] {
      handle_datagram(log_socket, &Control::Impl::handle_log_datagram);
    });
    bind_socket(&log_socket, 4005, "Log");

    leftWindow.setWindowTitle("InTex Live Feed 0");
    rightWindow.setWindowTitle("InTex Live Feed 1");

    QObject::connect(&client, &IntexRpcClient::gpioChanged,
                     [this](const InTexHW hw, const bool state) {
                       switch (hw) {
                       case InTexHW::VALVE0:
                         intexWidget->onValve1Changed(state);
                         break;
                       case InTexHW::VALVE1:
                         intexWidget->onValve2Changed(state);
                         break;
                       }
                     });
    QObject::connect(intexWidget, &IntexWidget::depressurizeRequest,
                     [this](const bool depressurize, auto cb) {
                       client.setGPIO(InTexHW::VALVE0, depressurize, cb);
                       client.setGPIO(InTexHW::VALVE1, depressurize, cb);
                     });
    QObject::connect(intexWidget, &IntexWidget::inflateRequest,
                     [this](const bool inflate, auto cb) {
                       client.setGPIO(InTexHW::VALVE0, inflate, cb);
                       client.setGPIO(InTexHW::VALVE1, false, cb);
                     });
    QObject::connect(intexWidget, &IntexWidget::equalizeRequest,
                     [this](const bool equalize, auto cb) {
                       client.setGPIO(InTexHW::VALVE0, false, cb);
                       client.setGPIO(InTexHW::VALVE1, equalize, cb);
                     });
    QObject::connect(intexWidget, &IntexWidget::valve1Request,
                     [this](const bool open) {
                       client.setGPIO(InTexHW::VALVE0, open, [](const auto) {});
                     });
    QObject::connect(intexWidget, &IntexWidget::valve2Request,
                     [this](const bool open) {
                       client.setGPIO(InTexHW::VALVE1, open, [](const auto) {});
                     });

    auto setBitrateChanged = [this](int) {
      const auto split = static_cast<unsigned>(splitSlider->value());
      const auto bitrate = static_cast<unsigned>(bitrateSlider->value());
      client.setBitrate(InTexFeed::FEED0, split * bitrate);
      client.setBitrate(InTexFeed::FEED1, (100 - split) * bitrate);
    };
    QObject::connect(bitrateSlider, &QSlider::valueChanged, setBitrateChanged);
    QObject::connect(splitSlider, &QSlider::valueChanged, setBitrateChanged);

    QObject::connect(&client, &IntexRpcClient::portChanged,
                     [this](const InTexService service, const uint16_t port) {
                       switch (service) {
                       case InTexService::VIDEO_FEED0:
                         videoControl.setPort(VideoStreamControl::Stream::Left,
                                              port);
                         break;
                       case InTexService::VIDEO_FEED1:
                         videoControl.setPort(VideoStreamControl::Stream::Right,
                                              port);
                         break;
                       }
                     });
  }

  ~Impl() { log_instance = nullptr; }
  void switchWidgets() { videoControl.switchWidgets(); }
  void switchWindows() { videoControl.switchWindows(); }
};

template <typename Reconnect, typename Idr>
static QWidget *setupVideoControls(Reconnect &&reconnector, Idr &&idr) {
  auto portLabel = new QLabel("Port:");
  portLabel->setSizePolicy(QSizePolicy::Policy::Fixed,
                           QSizePolicy::Policy::Fixed);

  auto port = new QLineEdit;
  port->setValidator(new QIntValidator(0, 0xffff));
  port->setSizePolicy(QSizePolicy::Policy::Preferred,
                      QSizePolicy::Policy::Fixed);

  auto reconnect = new QPushButton("Reconnect");
  reconnect->setSizePolicy(QSizePolicy::Policy::Preferred,
                           QSizePolicy::Policy::Fixed);
  QObject::connect(reconnect, &QPushButton::clicked,
                   [ port, reconnector = std::move(reconnector) ] {
                     reconnector(port->text().toInt());
                   });

  auto portControls = new QFrame;
  auto portControlsLayout = new QHBoxLayout(portControls);

  portControlsLayout->addWidget(portLabel);
  portControlsLayout->addWidget(port);
  portControlsLayout->addWidget(reconnect);

  auto iFrameControls = new QWidget;
  auto iFrameControlsLayout = new QHBoxLayout(iFrameControls);
  iFrameControlsLayout->setContentsMargins(0, 0, 0, 0);

  auto iFrameLabel = new QLabel("I-Frame periodicity [s]:");
  auto iFrameEdit = new QLineEdit;
  iFrameEdit->setValidator(new QDoubleValidator(0.0, 100.0, 2));

  iFrameControlsLayout->addWidget(iFrameLabel);
  iFrameControlsLayout->addWidget(iFrameEdit);

  auto idrControls = new QWidget;
  auto idrControlsLayout = new QHBoxLayout(idrControls);
  idrControlsLayout->setContentsMargins(0, 0, 0, 0);

  auto idrLabel = new QLabel("IDR periodicity [s]:");
  auto idrEdit = new QLineEdit;
  idrEdit->setValidator(new QDoubleValidator(0.0, 100.0, 2));

  QObject::connect(
      idrEdit, &QLineEdit::textChanged,
      [idr = std::move(idr)](const QString &text) { qDebug() << text; });

  idrControlsLayout->addWidget(idrLabel);
  idrControlsLayout->addWidget(idrEdit);

  auto videoControls = new QFrame;
  videoControls->setFrameShape(QFrame::StyledPanel);
  auto videoControlsLayout = new QVBoxLayout(videoControls);

  videoControlsLayout->addWidget(portControls);
  videoControlsLayout->addWidget(iFrameControls);
  videoControlsLayout->addWidget(idrControls);

  return videoControls;
}

Control::Control(QWidget *parent)
    : QMainWindow(parent), d_(std::make_unique<Control::Impl>(this)) {
  setWindowTitle(QCoreApplication::applicationName());

  auto mainMenu = menuBar()->addMenu(tr("Menu"));
  mainMenu->addAction(tr("C&onnect"), this, SLOT(onConnect()),
                      QKeySequence::Open);
  mainMenu->addAction(tr("Disconnect"), this, SLOT(onDisconnect()),
                      QKeySequence::Close);

  mainMenu->addAction(tr("Preferences"));

  auto viewMenu = menuBar()->addMenu(tr("View"));
  auto viewAction = viewMenu->addAction(tr("Show Video Controls"), this,
                                        SLOT(showVideoControls(bool)),
                                        QKeySequence(tr("Ctrl+k")));
  viewAction->setCheckable(true);
  viewAction->setChecked(true);

  auto heater0Action = new QAction(tr("Inner heater"), nullptr);
  heater0Action->setCheckable(true);
  connect(heater0Action, &QAction::triggered,
          [ heater0Action, &client = d_->client ](const bool on) {
            gpio_callback(heater0Action, InTexHW::HEATER0, client, on);
          });

  auto heater1Action = new QAction(tr("Outer heater"), nullptr);
  heater1Action->setCheckable(true);
  connect(heater1Action, &QAction::triggered,
          [ heater1Action, &client = d_->client ](const bool on) {
            gpio_callback(heater1Action, InTexHW::HEATER1, client, on);
          });

  auto burnwireAction = new QAction(tr("Burnwire"), nullptr);
  burnwireAction->setCheckable(true);
  connect(burnwireAction, &QAction::triggered,
          [ burnwireAction, &client = d_->client ](const bool on) {
            gpio_callback(burnwireAction, InTexHW::BURNWIRE, client, on);
          });

  auto periphMenu = menuBar()->addMenu(tr("Peripherals"));
  periphMenu->addAction(heater0Action);
  periphMenu->addAction(heater1Action);
  periphMenu->addAction(burnwireAction);

  auto centralWidget = new QWidget;
  setCentralWidget(centralWidget);

  auto centralLayout = new QVBoxLayout(centralWidget);

  auto videoWidget = new QFrame;
  videoWidget->setFrameShape(QFrame::StyledPanel);
  auto videoLayout = new QHBoxLayout(videoWidget);

  auto leftVideo = new QFrame;
  //leftVideo->setFrameShape(QFrame::StyledPanel);
  auto leftVideoLayout = new QVBoxLayout(leftVideo);

  leftVideoLayout->addWidget(d_->leftVideoWidget);
  leftVideoLayout->addItem(
      new QSpacerItem(0, 0, QSizePolicy::Fixed, QSizePolicy::MinimumExpanding));
  d_->leftVideoControl = setupVideoControls(
      [this](int port) {
        d_->client.setPort(InTexService::VIDEO_FEED0,
                           static_cast<uint16_t>(port));
      },
      [] {});
  leftVideoLayout->addWidget(d_->leftVideoControl);

  auto rightVideo = new QFrame;
  //rightVideo->setFrameShape(QFrame::StyledPanel);
  auto rightVideoLayout = new QVBoxLayout(rightVideo);

  rightVideoLayout->addWidget(d_->rightVideoWidget);
  rightVideoLayout->addItem(
      new QSpacerItem(0, 0, QSizePolicy::Fixed, QSizePolicy::MinimumExpanding));
  d_->rightVideoControl = setupVideoControls(
      [this](int port) {
        d_->client.setPort(InTexService::VIDEO_FEED1,
                           static_cast<uint16_t>(port));
      },
      [] {});
  rightVideoLayout->addWidget(d_->rightVideoControl);

  videoLayout->addWidget(leftVideo);
  videoLayout->addWidget(rightVideo);

  connect(d_->leftVideoWidget, &VideoWidget::widgetClicked, &d_->leftWindow,
          &VideoWindow::show);
  connect(d_->rightVideoWidget, &VideoWidget::widgetClicked, &d_->rightWindow,
          &VideoWindow::show);

  centralLayout->addWidget(videoWidget);

  auto controlWidget = new QFrame;
  controlWidget->setFrameShape(QFrame::StyledPanel);
  auto controlLayout = new QHBoxLayout(controlWidget);

  auto bitrateLabel = new QLabel;
  bitrateLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

  d_->bitrateSlider->setMinimum(1);
  d_->bitrateSlider->setMaximum(1000);
  d_->bitrateSlider->setSingleStep(1);
  d_->bitrateSlider->setPageStep(10);
  d_->bitrateSlider->setSizePolicy(QSizePolicy::MinimumExpanding,
                                   QSizePolicy::Fixed);

  auto bitrateUpdate = [bitrateLabel](int bitrate) {
    bitrateLabel->setText(QString("Bitrate: %1 kBit/s").arg(bitrate));
  };
  connect(d_->bitrateSlider, &QSlider::sliderMoved, bitrateUpdate);
  connect(d_->bitrateSlider, &QSlider::valueChanged, bitrateUpdate);

  d_->bitrateSlider->setValue(800);
  d_->bitrateSlider->setTracking(false);

  auto splitLabel = new QLabel;
  bitrateLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

  d_->splitSlider->setMinimum(1);
  d_->splitSlider->setMaximum(100);
  d_->splitSlider->setSingleStep(1);
  d_->splitSlider->setPageStep(10);
  d_->splitSlider->setSizePolicy(QSizePolicy::MinimumExpanding,
                                 QSizePolicy::Fixed);

  auto splitUpdate = [splitLabel](int split) {
    splitLabel->setText(
        QString("Split: %1/%2")
            .arg(QString::number(split), QString::number(100 - split)));
  };
  connect(d_->splitSlider, &QSlider::sliderMoved, splitUpdate);
  connect(d_->splitSlider, &QSlider::valueChanged, splitUpdate);

  d_->splitSlider->setValue(50);
  d_->splitSlider->setTracking(false);

  auto newFileButton = new QPushButton("New File");
  connect(newFileButton, &QPushButton::clicked, [this] {
    d_->client.next(InTexService::VIDEO_FEED0);
    d_->client.next(InTexService::VIDEO_FEED1);
  });

  controlLayout->addWidget(bitrateLabel);
  controlLayout->addWidget(d_->bitrateSlider);
  controlLayout->addWidget(splitLabel);
  controlLayout->addWidget(d_->splitSlider);
  controlLayout->addWidget(newFileButton);

  centralLayout->addWidget(controlWidget);
  centralLayout->addWidget(d_->intexWidget);
}

Control::~Control() = default;

void Control::onConnect() {}
void Control::onDisconnect() {}
void Control::switchWidgets() { d_->switchWidgets(); }
void Control::switchWindows() { d_->switchWindows(); }
void Control::showVideoControls(bool show) {
  d_->leftVideoControl->setVisible(show);
  d_->rightVideoControl->setVisible(show);
}

void onBusMessage(const QGst::MessagePtr &message);

#include "moc_Control.cpp"
