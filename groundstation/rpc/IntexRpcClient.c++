#include "IntexRpcClient.h"

IntexRpcClient::IntexRpcClient(std::string host, const unsigned port)
    : client(host.c_str(), port), intex(client.getMain<Command>()) {}

IntexRpcClient::~IntexRpcClient() noexcept {
  try {
    client.~EzRpcClient();
  } catch (...) {
  }
}

#pragma clang diagnostic ignored "-Wpadded"

void IntexRpcClient::setPort(const InTexService service, const uint16_t port) {
  auto request = intex.setPortRequest();
  request.setService(service);
  request.setPort(port);
  request.send()
      .then(
          [this, service, port](auto &&) { Q_EMIT portChanged(service, port); },
          [this](auto &&exception) {
            Q_EMIT log(exception.getDescription().cStr());
          })
      .detach([this](auto &&exception) {
        Q_EMIT log(exception.getDescription().cStr());
      });
}

void IntexRpcClient::setGPIO(const InTexHW hw, const bool state,
                             std::function<void(bool)> success) {
  auto request = intex.setGPIORequest();
  request.setPort(hw);
  request.setOn(state);
  request.send()
      .then(
          [this, hw, success, state](auto &&) {
            success(true);
            Q_EMIT log("Success");
            Q_EMIT gpioChanged(hw, state);
          },
          [this, hw, state, success](auto &&e) {
            success(false);
            Q_EMIT gpioChanged(hw, !state);
            Q_EMIT log(e.getDescription().cStr());
          })
      .detach([this, success](auto &&e) {
        success(false);
        Q_EMIT log(e.getDescription().cStr());
      });
}

void IntexRpcClient::setBitrate(const InTexFeed feed, const unsigned bitrate) {
  auto request = intex.setBitrateRequest();
  request.setFeed(feed);
  request.setBitrate(bitrate);
  request.send().detach([this](auto &&exception) {
    Q_EMIT log(exception.getDescription().cStr());
  });
}

#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wundefined-reinterpret-cast"
#include "moc_IntexRpcClient.cpp"
