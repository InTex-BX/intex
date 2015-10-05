#pragma once

#include <functional>

#include <QString>
#include <QObject>
#include <QByteArray>
#include <QVector>
#include <QUdpSocket>
#include <QAbstractSocket>

#include <kj/debug.h>
#include <kj/array.h>
#include <capnp/serialize.h>
#include <capnp/common.h>
#include <capnp/endian.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wweak-vtables"
#pragma clang diagnostic ignored "-Wnon-virtual-dtor"
#include "rpc/intex.capnp.h"
#pragma clang diagnostic pop

namespace intex {

class LogAdapter : public QObject {
  Q_OBJECT

public:
  LogAdapter &operator<<(const QString &msg);

  // clang-format off
Q_SIGNALS:
  void log(QString);
  // clang-format on
};

enum class Subsystem { Video0, Video1, Telemetry, Log };
QString storageLocation(const enum Subsystem subsys, unsigned int *last =
    nullptr);
QString deviceName(const enum Subsystem subsys);

static constexpr const char *to_string(const AutoAction action) {
  switch (action) {
  case AutoAction::INFLATE:
    return "Inflate";
  case AutoAction::MEASURE:
    return "Measure";
  case AutoAction::DEFLATE:
    return "Deflate";
  }
}

class QByteArrayMessageReader : public capnp::MessageReader {
  QByteArray &buffer;
  QVector<kj::ArrayPtr<const ::capnp::word>> segments;

public:
  QByteArrayMessageReader(QByteArray &buffer_, capnp::ReaderOptions options =
                                                   capnp::ReaderOptions());
  kj::ArrayPtr<const capnp::word> getSegment(uint id) override;
};

void bind_socket(QAbstractSocket *socket, quint16 port, QString what);
void handle_datagram(QUdpSocket &socket,
                     std::function<void(QByteArray &)> handler);
}

QDebug operator<<(QDebug dbg, const InTexHW hw);

// experiment: "172.16.18.162";
// ground station: "172.16.18.163";
static constexpr const char *groundstation_host() { return "grace.local"; }
static constexpr const char *intex_host() {
#ifdef BUILD_ON_RASPBERRY
  return "localhost";
#else
  return "intex.local";
#endif
}
static constexpr uint16_t intex_control_port() { return 1234; }
static constexpr uint16_t intex_auto_port() { return 32468; }

