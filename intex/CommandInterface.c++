#include <iostream>
#include <stdexcept>
#include <thread>
#include <chrono>

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QTime>
#include <QObject>

#include "CommandInterface.h"
#include "VideoStreamSourceControl.h"
#include "intex.h"

InTexServer *server_instance = nullptr;

InTexServer::InTexServer(QString host)
    : client("127.0.0.1"), control(host, 54431) {
  QObject::connect(&syslog_socket, &QAbstractSocket::connected, [this] {
    logs.push_back(std::make_unique<QTextStream>(&syslog_socket));
  });
  setupLogStream(4005);
  setupLogFiles();
}

InTexServer::~InTexServer() { server_instance = nullptr; }

void InTexServer::syslog(QtMsgType type, const QString &msg) {
  auto prefix = QTime::currentTime().toString(Qt::ISODate);
  switch (type) {
  case QtDebugMsg:
    prefix += " [DD] ";
    break;
  case QtWarningMsg:
    prefix += " [WW] ";
    break;
  case QtCriticalMsg:
    prefix += " [CC] ";
    break;
  case QtFatalMsg:
    prefix += " [EE] ";
    break;
  }

  for (const auto &log : logs) {
    *log << prefix << msg << endl;
  }
}

kj::Promise<void> InTexServer::setPort(SetPortContext context) {
  std::cout << __PRETTY_FUNCTION__ << " "
            << static_cast<int>(context.getParams().getService()) << " "
            << context.getParams().getPort() << std::endl;
  auto params = context.getParams();
  const auto port = params.getPort();
  switch (params.getService()) {
  case InTexService::LOG:
    setupLogStream(port);
  }
  throw std::runtime_error("Port not implemented.");
}

kj::Promise<void> InTexServer::setGPIO(SetGPIOContext context) {
  using namespace std::literals::chrono_literals;
  std::cout << __PRETTY_FUNCTION__ << std::endl;
  std::this_thread::sleep_for(0.1s);
  auto params = context.getParams();
  switch (params.getPort()) {
  case InTexHW::VALVE0:
    intex::hw::Valve::pressureTankValve().set(params.getOn());
    return kj::READY_NOW;
  case InTexHW::VALVE1:
    intex::hw::Valve::outletValve().set(params.getOn());
    return kj::READY_NOW;
  case InTexHW::HEATER0:
    intex::hw::Heater::innerHeater().set(params.getOn());
    return kj::READY_NOW;
  case InTexHW::HEATER1:
    intex::hw::Heater::outerHeater().set(params.getOn());
    return kj::READY_NOW;
  case InTexHW::BURNWIRE:
    intex::hw::Burnwire::burnwire().set(params.getOn());
    return kj::READY_NOW;
  case InTexHW::MINIVNA:
    intex::hw::MiniVNA::miniVNA().set(params.getOn());
    return kj::READY_NOW;
  case InTexHW::USBHUB:
    intex::hw::USBHub::usbHub().set(params.getOn());
    return kj::READY_NOW;
  }
  throw std::runtime_error("GPIO not implemented.");
}

kj::Promise<void> InTexServer::start(StartContext context) {
  control.videoStart(context.getParams().getFeed());
  return kj::READY_NOW;
}

kj::Promise<void> InTexServer::stop(StopContext context) {
  control.videoStop(context.getParams().getFeed());
  return kj::READY_NOW;
}

kj::Promise<void> InTexServer::next(NextContext context) {
  control.videoNext(context.getParams().getFeed());
  return kj::READY_NOW;
}

kj::Promise<void> InTexServer::setVolume(SetVolumeContext context) {
  auto params = context.getParams();
  control.setVolume(params.getFeed(), params.getVolume());
  return kj::READY_NOW;
}

kj::Promise<void> InTexServer::setBitrate(SetBitrateContext context) {
  auto params = context.getParams();
  control.setVolume(params.getFeed(), params.getBitrate());
  return kj::READY_NOW;
}

kj::Promise<void> InTexServer::launch(LaunchContext) {
  control.launched();
  return kj::READY_NOW;
}

kj::Promise<void> InTexServer::nva(NvaContext) {
  control.measureAntenna();
  return kj::READY_NOW;
}

void InTexServer::setupLogStream(const uint16_t port) {
  syslog_socket.connectToHost(client.c_str(), port, QIODevice::WriteOnly,
                              QAbstractSocket::IPv4Protocol);
}

void InTexServer::setupLogFiles() {
  auto date = QDateTime::currentDateTime().toString(Qt::ISODate);

  try {
    auto file = std::make_unique<QFile>(storageLocation(intex::Subsystem::Log));
    if (file == nullptr) {
      throw std::runtime_error("Could not create QFile object.");
    }

    if (!file->open(QIODevice::WriteOnly | QIODevice::Append |
                    QIODevice::Text)) {
      throw std::runtime_error("Could not open log file " +
                               file->fileName().toStdString() +
                               " for writing.");
    }

    auto stream = std::make_unique<QTextStream>(file.get());
    if (stream == nullptr) {
      throw std::runtime_error(
          "Could not create QTextStream object for log file " +
          file->fileName().toStdString() + ".");
    }

    files.push_back(std::move(file));
    logs.push_back(std::move(stream));

    *logs.back() << "Log created at " << date << endl;
  } catch (const std::runtime_error &e) {
    qCritical() << QString::fromStdString(e.what());
  }
}
