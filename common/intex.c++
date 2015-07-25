#include <stdexcept>
#include <sstream>

#include <cerrno>
#include <cstring>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QTextStream>

#include "intex.h"

boost::asio::ip::address intex_ip() {
  return boost::asio::ip::address::from_string("172.16.18.162");
}

boost::asio::ip::address groundstation_ip() {
  return boost::asio::ip::address::from_string("172.16.18.163");
}

boost::asio::ip::tcp::endpoint intex_control() {
  return boost::asio::ip::tcp::endpoint(intex_ip(), intex_control_port);
}

namespace intex {
static QString subdirectory(const enum Subsystem subsys) {
  switch (subsys) {
  case Subsystem::Video0:
  case Subsystem::Video1:
    return "video";
  case Subsystem::Log:
    return "log";
  case Subsystem::Temperature:
    return "temperature";
  case Subsystem::Pressure:
    return "pressure";
  }
}

static QString suffix(const enum Subsystem subsys) {
  switch (subsys) {
  case Subsystem::Video0:
  case Subsystem::Video1:
    return ".mkv";
  case Subsystem::Log:
    return ".log";
  case Subsystem::Temperature:
    return "";
  case Subsystem::Pressure:
    return "";
  }
}

QString storageLocation(int replica, const enum Subsystem subsys,
                        unsigned int *last) {
  const unsigned max_files = 100000;
  const unsigned reboot = 0;
  QFileInfo basepath(
      QString("/media/usb%1/%2").arg(replica).arg(subdirectory(subsys)));

  if (!basepath.exists()) {
    throw std::runtime_error("Directory " +
                             basepath.absolutePath().toStdString() +
                             " does not exist.");
  }

  if (!basepath.isDir()) {
    throw std::runtime_error(basepath.absolutePath().toStdString() +
                             " is not a directory.");
  }

  auto directory = basepath.dir();
  auto suffix_ = suffix(subsys);
  auto start = (last != nullptr) ? *last : 0;

  for (auto fileno = start; fileno < max_files; ++fileno) {
    auto file = directory.filePath(QString("%1-%2.%3")
                                       .arg(reboot, 3, 10, QChar('0'))
                                       .arg(fileno, 5, 10, QChar('0'))
                                       .arg(suffix_));
    if (QFileInfo::exists(file)) {
      qDebug() << "Choosing file" << file;
      if (last != nullptr)
        *last = fileno;
      return file;
    } else {
      qDebug() << "Skipping existing file" << file;
    }
  }

  throw std::runtime_error("Maximum number of files reached.");
}
}
