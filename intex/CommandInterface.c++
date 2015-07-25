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

#include "CommandInterface.h"
#include "VideoStreamSourceControl.h"
#include "intex.h"

InTexServer *server_instance = nullptr;

InTexServer::InTexServer()
    : source0("127.0.0.1", "5000", ""), valve0(intex::hw::config::valve0),
      valve1(intex::hw::config::valve1) {
  server_instance = this;

  syslog_socket.connectToHost("localhost", 4003, QIODevice::WriteOnly,
                              QAbstractSocket::IPv4Protocol);
  syslog_socket.waitForConnected();
  logs.push_back(std::make_unique<QTextStream>(&syslog_socket));
  setupLogFiles();
}

InTexServer::~InTexServer() { server_instance = nullptr; }

void InTexServer::syslog(QtMsgType type, const QString &msg) {
  auto prefix = QTime::currentTime().toString("hh:mm:ss");
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
  std::cerr << prefix.toStdString() << msg.toStdString() << std::endl;
}

kj::Promise<void> InTexServer::setPort(SetPortContext context) {
  std::cout << __PRETTY_FUNCTION__ << " "
            << static_cast<int>(context.getParams().getService()) << " "
            << context.getParams().getPort() << std::endl;
  auto params = context.getParams();
  switch (params.getService()) {
  case InTexService::VIDEO_FEED0:
    source0.setPort(params.getPort());
    return kj::READY_NOW;
    break;
  }
  throw std::runtime_error("Port not implemented.");
}

kj::Promise<void> InTexServer::setGPIO(SetGPIOContext context) {
  using namespace std::chrono;
  std::cout << __PRETTY_FUNCTION__ << std::endl;
  std::this_thread::sleep_for(0.1s);
  auto params = context.getParams();
  switch (params.getPort()) {
  case InTexHW::VALVE0:
    valve0.set(params.getOn());
    return kj::READY_NOW;
  case InTexHW::VALVE1:
    valve1.set(params.getOn());
    return kj::READY_NOW;
  }
  throw std::runtime_error("GPIO not implemented.");
}


void InTexServer::setupLogFiles() {
  auto date = QDateTime::currentDateTime().toString(Qt::ISODate);

  for (unsigned int log = 0; log < nr_log_locations; ++log) {
    try {
      auto file =
          std::make_unique<QFile>(storageLocation(log, intex::Subsystem::Log));
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
      continue;
    }
  }
}
