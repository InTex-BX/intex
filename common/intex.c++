#include <stdexcept>
#include <sstream>
#include <chrono>

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
#include <QTimer>

#include "intex.h"


namespace intex {
LogAdapter &LogAdapter::operator<<(const QString &msg) {
  Q_EMIT log(msg);
  return *this;
}

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
  case Subsystem::Log:
    return "log";
  }

  throw std::runtime_error("deviceName for subsystem " +
                           std::to_string(static_cast<int>(subsys)) +
                           " not implemented.");
}

static QFileInfo initializeDataDirectory(const enum Subsystem subsys) {
#ifdef BUILD_ON_RASPBERRY
  QString path("/media/usb-raid/%1");
#else
  QString path("/Volumes/Intex/data/%1");
#endif
  return QFileInfo(path.arg(subdirectory(subsys)));
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

QByteArrayMessageReader::QByteArrayMessageReader(QByteArray &buffer_,
                                                 capnp::ReaderOptions options)
    : capnp::MessageReader(options), buffer(buffer_) {
  auto buffersize = static_cast<size_t>(buffer.size());

  if (buffersize < sizeof(capnp::word)) {
    /* empty message */
    return;
  }

  const capnp::_::WireValue<uint32_t> *table =
      reinterpret_cast<const capnp::_::WireValue<uint32_t> *>(buffer.data());

  uint segmentCount = table[0].get() + 1;
  size_t offset = segmentCount / 2u + 1u;

  KJ_REQUIRE(buffersize >= offset,
             "Message ends prematurely in segment table.") {
    return;
  }

  for (uint segment = 0; segment < segmentCount; ++segment) {
    uint segmentSize = table[segment + 1].get();

    KJ_REQUIRE(buffersize >= offset + segmentSize,
               "Message ends prematurely in first segment.") {
      return;
    }

    segments.append(kj::ArrayPtr<const capnp::word>(
        reinterpret_cast<const capnp::word *>(buffer.data()) + offset,
        offset + segmentSize));

    offset += segmentSize;
  }
}

kj::ArrayPtr<const capnp::word> QByteArrayMessageReader::getSegment(uint id) {
  auto id_ = static_cast<int>(id);
  if (id_ < segments.size()) {
    return segments.at(id_);
  } else {
    return nullptr;
  }
}

void handle_datagram(
    QUdpSocket &socket,
    std::function<void(QByteArray &, QHostAddress &, quint16)> handler) {
  for (; socket.hasPendingDatagrams();) {
    auto size = socket.pendingDatagramSize();
    if (size < 0) {
      qDebug() << "No datagram ready.";
      continue;
    }

    QByteArray buffer;
    QHostAddress sender;
    quint16 port;
    buffer.resize(static_cast<int>(size));
    auto ret =
        socket.readDatagram(buffer.data(), buffer.size(), &sender, &port);
    if (ret < 0) {
      qCritical() << "Could not read datagram.";
      return;
    }

    handler(buffer, sender, port);
  }
}

void bind_socket(QAbstractSocket *socket, quint16 port, QString what) {
  if (socket->bind(QHostAddress::Any, port)) {
    qDebug().nospace() << "Listening for " << what << " datagrams on "
                       << socket->localAddress() << ":" << socket->localPort();
  } else {
    using namespace std::chrono;
    using namespace std::literals::chrono_literals;
    qDebug() << "Error binding socket to receive " << what
             << " datagrams:" << socket->error() << ". Retrying";
    QTimer::singleShot(
        duration_cast<milliseconds>(5s).count(),
        [socket, port, what] { bind_socket(socket, port, what); });
  }
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

#pragma clang diagnostic ignored "-Wundefined-reinterpret-cast"
#include "moc_intex.cpp"
