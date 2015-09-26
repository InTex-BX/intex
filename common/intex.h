#pragma once

#include <functional>

#include <QString>

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

