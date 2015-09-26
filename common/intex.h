#pragma once

#include <functional>

#include <QString>

#include "asio.h"
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

boost::asio::ip::address intex_ip();
boost::asio::ip::address groundstation_ip();

QDebug operator<<(QDebug dbg, const InTexHW hw);
