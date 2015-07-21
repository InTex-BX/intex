#include "intex.h"

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

boost::asio::ip::address intex_ip() {
  return boost::asio::ip::address::from_string("172.16.18.162");
}

boost::asio::ip::address groundstation_ip() {
  return boost::asio::ip::address::from_string("172.16.18.163");
}

boost::asio::ip::tcp::endpoint intex_control() {
  return boost::asio::ip::tcp::endpoint(intex_ip(), intex_control_port);
}

unsigned next_file(const QString &path, filename_formatter fname) {
  const unsigned max_files = 100000;
  unsigned result;

  QFileInfo directory(path);
  if (!directory.exists()) {
    throw std::runtime_error("Directory " + path.toStdString() +
                             " does not exist.");
  }

  if (!directory.isDir()) {
    throw std::runtime_error(path.toStdString() + " is not a directory.");
  }

  for (result = 0; result < max_files; ++result) {
    auto file = directory.dir().filePath(fname(result));
    if (!QFileInfo::exists(file)) {
      qDebug() << "File " << file << " does not yet exist.";
      return result;
    } else {
      qDebug() << "File " << file << " already exist.";
    }
  }

  throw std::runtime_error("Maximum number of files reached.");
}
