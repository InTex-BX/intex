#pragma once
#include <memory>
#include <vector>
#include <string>

#include <QFile>
#include <QObject>
#include <QString>
#include <QtGlobal>
#include <QUdpSocket>
#include <QTextStream>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnon-virtual-dtor"
#pragma clang diagnostic ignored "-Wweak-vtables"
#include "rpc/intex.capnp.h"
#pragma clang diagnostic pop

#include "IntexHardware.h"
#include "ExperimentControl.h"

class InTexServer final : public Command::Server {
  static constexpr int max_logfiles = 10000;
  std::string client;

  QUdpSocket syslog_socket;
  std::vector<std::unique_ptr<QFile>> files;
  std::vector<std::unique_ptr<QTextStream>> logs;

  intex::ExperimentControl control;

  void setupLogStream(const uint16_t port);
  void setupLogFiles();

public:
  InTexServer(QString host);
  ~InTexServer();
  void syslog(QtMsgType type, const QString &msg);
  kj::Promise<void> setPort(SetPortContext context) override;
  kj::Promise<void> setBitrate(SetBitrateContext context) override;
  kj::Promise<void> setVolume(SetVolumeContext context) override;
  kj::Promise<void> setGPIO(SetGPIOContext context) override;
  kj::Promise<void> start(StartContext context) override;
  kj::Promise<void> stop(StopContext context) override;
  kj::Promise<void> next(NextContext context) override;
  kj::Promise<void> launch(LaunchContext context) override;
};

extern InTexServer *server_instance;
