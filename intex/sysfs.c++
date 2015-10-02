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

static auto each_interface(QDir& directory) {
  QRegularExpression interface_pattern("^\\d+-\\d+(\\.\\d)+:[0-9]+.[0-9]+$");
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

static auto enumerate_webcams() {
  auto dir = QDir("/sys/bus/usb/devices");

  QVector<device_t> webcams;

  for (const auto &hub : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
    dir.cd(hub);
    if (is_webcam(dir))
      webcams += each_interface(dir);
    dir.cdUp();
  }

  return webcams;
}

QPair<QString, QString> findDevice(int idx) {
  auto devices = enumerate_webcams();

  if (idx < devices.size())
    return devices.at(idx);

  throw std::runtime_error("Webcam " + std::to_string(idx) + " not found.");
}
