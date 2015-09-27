#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStringList>
#include <QTextStream>
#include <QVector>

#include <stdexcept>

#include "sysfs.h"

using device_t = QPair<QString, QString>;

static bool is_webcam(QDir &directory) {
  QFileInfo file(directory, "product");

  if (!file.exists())
    return false;

  QRegularExpression deviceName("^HD Pro Webcam C920$");
  QFile product(file.absoluteFilePath());
  product.open(QIODevice::ReadOnly);
  QTextStream is(&product);

  return deviceName.match(is.readLine()).hasMatch();
}

static bool is_videointerface(QDir &directory) {
  return directory.exists("video4linux");
}

static bool is_audiointerface(QDir &directory) {
  return directory.exists("sound");
}

static QString video_device_name(QDir directory) {
  QRegularExpression device_pattern("^video[0-9]+$");

  directory.cd("video4linux");

  for (const auto &device : directory.entryList()) {
    if (device_pattern.match(device).hasMatch()) {
      return "/dev/" + device;
    }
  }

  auto cause = "Directory " + directory.absolutePath() +
               " is not a video interface path";
  throw std::runtime_error(cause.toStdString());
}

static QString audio_device_name(QDir directory) {
  QRegularExpression device_pattern("^card(?<id>[0-9]+)$");

  directory.cd("sound");

  for (const auto &device : directory.entryList()) {
    auto match = device_pattern.match(device);
    if (match.hasMatch()) {
      return "hw:" + match.captured("id");
    }
  }

  auto cause = "Directory " + directory.absolutePath() +
               " is not a video interface path";
  throw std::runtime_error(cause.toStdString());
  
}

static auto each_interface(QDir& directory, const QString &device) {
  QRegularExpression interface_pattern("^" + device + ":[0-9]+.[0-9]+$");
  device_t devices;

  for (const auto &interface : directory.entryList()) {
    if (interface_pattern.match(interface).hasMatch()) {
      directory.cd(interface);
      if (is_videointerface(directory))
        devices.first = video_device_name(directory);
      if (is_audiointerface(directory))
        devices.second = audio_device_name(directory);
      directory.cdUp();
    }
  }

  if (devices.first.isEmpty() || devices.second.isEmpty())
    throw std::runtime_error("video device, audio device or both missing.");

  return devices;
}

static auto each_device(QDir &directory, QString bus) {
  QRegularExpression device_pattern("^" + bus + ".[0-9]+$");
  QVector<device_t> webcams;

  for (const auto &device : directory.entryList()) {
    if (device_pattern.match(device).hasMatch()) {
      directory.cd(device);
      if (is_webcam(directory)) {
        try {
          webcams.append(each_interface(directory, device));
        } catch (const std::runtime_error &e) {
          qCritical() << e.what();
        }
      }
      directory.cdUp();
    }
  }

  return webcams;
}

static auto each_hc_port(QDir &directory) {
  QRegularExpression hcport_pattern("^[0-9]+-[0-9]+$");

  QVector<device_t> webcams;

  for (const auto &port : directory.entryList()) {
    if (hcport_pattern.match(port).hasMatch()) {
      directory.cd(port);
      webcams += each_device(directory, port);
      directory.cdUp();
    }
  }

  return webcams;
}

static auto enumerate_webcams() {
  QRegularExpression hub_pattern("^usb[0-9]+");
  auto dir = QDir("/sys/bus/usb/devices");

  QVector<device_t> webcams;

  for (const auto &hub : dir.entryList()) {
    if (hub_pattern.match(hub).hasMatch()) {
      qDebug() << "Found hub" << hub;
      dir.cd(hub);
      webcams += each_hc_port(dir);
      dir.cdUp();
    }
  }

  return webcams;
}

QPair<QString, QString> findDevice(unsigned idx) {
  auto devices = enumerate_webcams();

  if (idx < devices.size())
    return devices.at(idx);

  throw std::runtime_error("Webcam " + std::to_string(idx) + " not found.");
}
