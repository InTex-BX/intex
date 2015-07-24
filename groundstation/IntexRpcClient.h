#pragma once

#include <string>
#include <functional>

#include <QObject>
#include <QString>

#include "rpc/ez-rpc.h"
#include "rpc/intex.capnp.h"

class IntexRpcClient : public QObject {
  Q_OBJECT

  std::string host_;
  intex::rpc::EzRpcClient client;
  Command::Client intex;

public:
  IntexRpcClient(std::string host, const unsigned port);
  ~IntexRpcClient() noexcept;

public Q_SLOTS:
  void setPort(const InTexService service, const uint16_t port);
  void setGPIO(const InTexHW hw, const bool open,
               std::function<void(bool)> succes);
  void setBitrate(const InTexFeed feed, const unsigned bitrate);

private Q_SLOTS:
  void onConnect();
  void onDisconnect();

  // clang-format off
Q_SIGNALS:
  void log(QString msg);
  void gpioChanged(const InTexHW hw, const bool state);
  void portChanged(const InTexService service, uint16_t port);
  // clang-format on
};
