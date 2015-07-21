#pragma once

#include <functional>

#include <QString>

#include "asio.h"

using filename_formatter = std::function<QString(unsigned)>;
unsigned next_file(const QString &path, filename_formatter fname);

boost::asio::ip::address intex_ip();
boost::asio::ip::address groundstation_ip();
