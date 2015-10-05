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
  void setVolume(const InTexFeed feed, const float volume);
  void start(const InTexFeed feed);
  void stop(const InTexFeed feed);
  void next(const InTexFeed feed);
  void launch();
  void nva();

private Q_SLOTS:
  void onConnect();
  void onDisconnect();

  // clang-format off
Q_SIGNALS:
  void gpioChanged(const InTexHW hw, const bool state);
  void portChanged(const InTexService service, uint16_t port);
  void connected();
  void disconnected();
  // clang-format on
};
