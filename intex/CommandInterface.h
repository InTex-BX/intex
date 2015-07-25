#pragma once
#include <memory>
#include <vector>

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

#include "VideoStreamSourceControl.h"
#include "IntexHardware.h"

class InTexServer final : public Command::Server {
  static constexpr unsigned int nr_log_locations = 2;
  static constexpr int max_logfiles = 10000;

  QUdpSocket syslog_socket;
  std::vector<std::unique_ptr<QFile>> files;
  std::vector<std::unique_ptr<QTextStream>> logs;

  VideoStreamSourceControl source0;
  intex::hw::Valve valve0;
  intex::hw::Valve valve1;

  void setupLogFiles();

public:
  InTexServer();
  ~InTexServer();
  void syslog(QtMsgType type, const QString &msg);
  kj::Promise<void> setPort(SetPortContext context) override;
  kj::Promise<void> setGPIO(SetGPIOContext context) override;
};

extern InTexServer *server_instance;
