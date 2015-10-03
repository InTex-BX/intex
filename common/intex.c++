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
#include <QStandardPaths>
#include <QTextStream>

#include "intex.h"


namespace intex {
static QString subdirectory(const enum Subsystem subsys) {
  switch (subsys) {
  case Subsystem::Video0:
  case Subsystem::Video1:
    return "video";
  case Subsystem::Telemetry:
    return "telemetry";
  case Subsystem::Log:
    return "log";
  }
}

static QString suffix(const enum Subsystem subsys) {
  switch (subsys) {
  case Subsystem::Video0:
  case Subsystem::Video1:
    return "mpeg";
  case Subsystem::Telemetry:
    return "data";
  case Subsystem::Log:
    return "log";
  }
}

QString deviceName(const enum Subsystem subsys) {
  switch (subsys) {
  case Subsystem::Video0:
    return "cam0";
  case Subsystem::Video1:
    return "cam1";
  case Subsystem::Telemetry:
    return "telementry";
  }

  throw std::runtime_error("deviceName for subsystem " +
                           std::to_string(static_cast<int>(subsys)) +
                           " not implemented.");
}

#ifndef BUILD_ON_RASPBERRY
static void cdCreateSubdir(QDir &directory, QString subdir) {
  if (!directory.cd(subdir)) {
    qDebug() << "Creating directory" << directory.filePath(subdir);
    directory.mkdir(subdir);
    if (!directory.cd(subdir))
      throw std::runtime_error(std::string("Could not create '") +
                               subdir.toLatin1().data() + "' directory in " +
                               directory.absolutePath().toLatin1().data());
  }
}
#endif

static QFileInfo initializeDataDirectory(const enum Subsystem subsys) {
#ifdef BUILD_ON_RASPBERRY
  return QFileInfo(
      QString("/media/usb-raid/%1").arg(subdirectory(subsys)));
#else
  QDir basePath{
      QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)};

  if(!basePath.exists()) {
    throw std::runtime_error(std::string("Document directory ") +
                             basePath.absolutePath().toLatin1().data() +
                             " does not exist.");
  }

  cdCreateSubdir(basePath, "intex");
  cdCreateSubdir(basePath, "data");
  cdCreateSubdir(basePath, subdirectory(subsys));
  return basePath.absolutePath();
#endif
}

QString storageLocation(const enum Subsystem subsys, unsigned int *last) {
  const unsigned max_files = 100000;
  const unsigned reboot = 0;
  QFileInfo basepath(initializeDataDirectory(subsys));

  if (!basepath.exists()) {
    qDebug() << "Directory " << basepath.absoluteFilePath()
             << " does not exist.";
    throw std::runtime_error("Directory " +
                             basepath.absoluteFilePath().toStdString() +
                             " does not exist.");
  }

  if (!basepath.isDir()) {
    qDebug() << basepath.absoluteFilePath() << " is not a directory.";
    throw std::runtime_error(basepath.absoluteFilePath().toStdString() +
                             " is not a directory.");
  }

  auto directory = QDir(basepath.filePath());
  auto suffix_ = suffix(subsys);
  auto devName = deviceName(subsys);
  auto start = (last != nullptr) ? *last : 0;

  for (auto fileno = start; fileno < max_files; ++fileno) {
    auto file = directory.filePath(QString("%1-%2-%3.%4")
                                       .arg(devName)
                                       .arg(reboot, 3, 10, QChar('0'))
                                       .arg(fileno, 5, 10, QChar('0'))
                                       .arg(suffix_));
    if (!QFileInfo::exists(file)) {
      qDebug() << "Choosing file" << file;
      if (last != nullptr)
        *last = fileno;
      return file;
    } else {
      qDebug() << "Skipping existing file" << file;
    }
  }

  qDebug() << "Maximum number of files reached.";
  throw std::runtime_error("Maximum number of files reached.");
}
}

QDebug operator<<(QDebug dbg, const InTexHW hw) {
  switch (hw) {
  case InTexHW::VALVE0:
    return dbg << "Pressure tank valve";
  case InTexHW::VALVE1:
    return dbg << "Outlet valve";
  case InTexHW::HEATER0:
    return dbg << "Inner heater";
  case InTexHW::HEATER1:
    return dbg << "Outer heater";
  case InTexHW::BURNWIRE:
    return dbg << "Burnwire";
  case InTexHW::MINIVNA:
    return dbg << "MiniVNA";
  case InTexHW::USBHUB:
    return dbg << "USB Hub";
  }
}
