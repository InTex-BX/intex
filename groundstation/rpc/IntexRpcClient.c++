#include <QDebug>

#include "IntexRpcClient.h"

IntexRpcClient::IntexRpcClient(std::string host, const unsigned port)
    : client(host.c_str(), port), intex(client.getMain<Command>()) {}

IntexRpcClient::~IntexRpcClient() noexcept {
  try {
    client.~EzRpcClient();
  } catch (...) {
  }
}

void IntexRpcClient::setPort(const InTexService service, const uint16_t port) {
  auto &waitScope = client.getWaitScope();
  auto request = intex.setPortRequest();
  request.setService(service);
  request.setPort(port);
  auto promise = request.send();
  try {
    auto response = promise.wait(waitScope);
  } catch (const kj::Exception &e) {
    qDebug() << e.getDescription().cStr();
  }
}

void IntexRpcClient::setGPIO(const InTexHW hw, const bool open,
                             std::function<void(bool)> cb) {
  qDebug() << static_cast<const int>(hw) << open;
  auto request = intex.setGPIORequest();
  request.setPort(hw);
  request.setOn(open);
  request.send()
      .then([cb](capnp::Response<Command::SetGPIOResults>) { cb(true); },
            [cb](kj::Exception &&e) {
              qDebug() << e.getDescription().cStr();
              cb(false);

            })
      .detach([](auto &&e) { qDebug() << e.getDescription().cStr(); });
}

void IntexRpcClient::setBitrate(const InTexFeed feed, const unsigned bitrate) {
  auto &waitScope = client.getWaitScope();
  auto request = intex.setBitrateRequest();
  request.setFeed(feed);
  request.setBitrate(bitrate);
  auto promise = request.send();
  try {
    auto response = promise.wait(waitScope);
  } catch (const kj::Exception &e) {
    qDebug() << e.getDescription().cStr();
  }
}

#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wundefined-reinterpret-cast"
#include "moc_IntexRpcClient.cpp"
#include "IntexRpcClient.moc"
