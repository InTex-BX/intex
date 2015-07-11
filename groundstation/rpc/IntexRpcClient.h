#pragma once

#include <string>
#include <functional>

#include <QObject>

#include "ez-rpc.h"
#include "intex.capnp.h"


class IntexRpcClient : public QObject {
  Q_OBJECT

  intex::rpc::EzRpcClient client;
  Command::Client intex;

public:
  IntexRpcClient(std::string host, const unsigned port);
  ~IntexRpcClient() noexcept;

public Q_SLOTS:
  void setPort(const InTexService service, const uint16_t port);
  void setGPIO(const InTexHW hw, const bool open, std::function<void(bool)> cb);
  void setBitrate(const InTexFeed feed, const unsigned bitrate);
};
