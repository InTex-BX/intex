#include "IntexRpcClient.h"

#include <QDebug>

IntexRpcClient::IntexRpcClient(std::string host, const unsigned port)
    : host_(std::move(host)), client(host_.c_str(), port),
      intex(client.getMain<Command>()) {
  connect(&client, &intex::rpc::EzRpcClient::connected, this,
          &IntexRpcClient::onConnect);
  connect(&client, &intex::rpc::EzRpcClient::disconnected, this,
          &IntexRpcClient::onDisconnect);
  connect(&client, &intex::rpc::EzRpcClient::connected, this,
          &IntexRpcClient::connected);
  connect(&client, &intex::rpc::EzRpcClient::disconnected, this,
          &IntexRpcClient::disconnected);
}

IntexRpcClient::~IntexRpcClient() noexcept try {
} catch (const kj::Exception &e) {
  qCritical() << e.getDescription().cStr();
} catch (const std::runtime_error &e) {
  qCritical() << e.what();
} catch (...) {
  qCritical() << "Unkown error occured" << __LINE__ << __PRETTY_FUNCTION__;
}

void IntexRpcClient::onConnect() { intex = client.getMain<Command>(); }
void IntexRpcClient::onDisconnect() { intex = client.getMain<Command>(); }

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

void IntexRpcClient::stop(const InTexService service) {}
void IntexRpcClient::start(const InTexService service) {}
void IntexRpcClient::next(const InTexService service) {
  auto request = intex.nextRequest();
  request.setService(service);
  request.send()
      .then(
          [service](auto &&) {
            qDebug() << "New file for service" << static_cast<int>(service)
                     << "created.";
          },
          [service](auto &&exception) {
            qDebug() << "New file for service" << static_cast<int>(service)
                     << "failed:" << exception.getDescription().cStr();
          })
      .detach([](auto &&exception) {
        qDebug() << "Detach failed:" << exception.getDescription().cStr();
      });
}

#pragma clang diagnostic ignored "-Wundefined-reinterpret-cast"
#include "moc_IntexRpcClient.cpp"
