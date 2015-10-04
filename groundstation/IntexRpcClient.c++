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
            qCritical() << exception.getDescription().cStr();
          })
      .detach([this](auto &&exception) {
        qCritical() << exception.getDescription().cStr();
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
            qDebug() << "Success";
            Q_EMIT gpioChanged(hw, state);
          },
          [this, hw, state, success](auto &&e) {
            success(false);
            Q_EMIT gpioChanged(hw, !state);
            qCritical() << e.getDescription().cStr();
          })
      .detach([this, success](auto &&e) {
        success(false);
        qCritical() << e.getDescription().cStr();
      });
}

void IntexRpcClient::setBitrate(const InTexFeed feed, const unsigned bitrate) {
  auto request = intex.setBitrateRequest();
  request.setFeed(feed);
  request.setBitrate(bitrate);
  request.send().detach([this](auto &&exception) {
    qCritical() << exception.getDescription().cStr();
  });
}

void IntexRpcClient::setVolume(const InTexFeed feed, const float volume) {
  auto request = intex.setVolumeRequest();
  request.setFeed(feed);
  request.setVolume(volume);
  request.send().detach([this](auto &&exception) {
    qCritical() << exception.getDescription().cStr();
  });
}

template <typename Request>
static void send_request(const InTexFeed feed, QString what,
                         Request &&request) {
  request.setFeed(feed);
  request.send()
      .then(
          [feed, what](auto &&) {
            qDebug() << what << "for feed" << static_cast<int>(feed)
                     << "created.";
          },
          [feed, what](auto &&exception) {
            qDebug() << what << "for feed" << static_cast<int>(feed)
                     << "failed:" << exception.getDescription().cStr();
          })
      .detach([what](auto &&exception) {
        qDebug() << what << "future detach failed:"
                 << exception.getDescription().cStr();
      });
}

void IntexRpcClient::stop(const InTexFeed feed) {
  send_request(feed, "Stop", intex.stopRequest());
}
void IntexRpcClient::start(const InTexFeed feed) {
  send_request(feed, "Start", intex.startRequest());
}
void IntexRpcClient::next(const InTexFeed feed) {
  send_request(feed, "New", intex.nextRequest());
}

void IntexRpcClient::launch() {
  auto request = intex.launchRequest();
  request.send().detach([this](auto &&exception) {
    qCritical() << exception.getDescription().cStr();
  });
}

#pragma clang diagnostic ignored "-Wundefined-reinterpret-cast"
#include "moc_IntexRpcClient.cpp"
