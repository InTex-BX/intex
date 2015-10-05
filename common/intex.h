#pragma once

#include <functional>

#include <QString>
#include <QByteArray>
#include <QVector>

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
enum class Subsystem { Video0, Video1, Telemetry, Log };
QString storageLocation(const enum Subsystem subsys, unsigned int *last =
    nullptr);
QString deviceName(const enum Subsystem subsys);
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

class QByteArrayMessageReader : public capnp::MessageReader {
  QByteArray &buffer;
  QVector<kj::ArrayPtr<const ::capnp::word>> segments;

public:
  QByteArrayMessageReader(QByteArray &buffer_, capnp::ReaderOptions options =
                                                   capnp::ReaderOptions());
  kj::ArrayPtr<const capnp::word> getSegment(uint id) override;
};

