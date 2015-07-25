#pragma once

#include <functional>

#include <QString>

#include "asio.h"

namespace intex {
enum class Subsystem { Video0, Video1, Log, Temperature, Pressure };
QString storageLocation(unsigned int replica, const enum Subsystem subsys,
                        unsigned int *last = nullptr);
QString deviceName(const enum Subsystem subsys);
}

boost::asio::ip::address intex_ip();
boost::asio::ip::address groundstation_ip();
