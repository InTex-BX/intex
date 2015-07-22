#include <sstream>
#include <stdexcept>
#include <iostream>

#include <QDebug>
#include <QFile>
#include <QString>
#include <QTextStream>

#include "VideoStreamSourceControl.h"

#include "intex.h"

static constexpr char encoderName[] = "encoder";
static constexpr char sinkName[] = "udpsink";

struct debug_tag {};

static QString make_downlink(const QString &host, const QString &port) {
  QString buf;
  QTextStream downlink(&buf);
  downlink << " ! queue ! omxh264dec "
           << " ! videoscale ! video/x-raw,width=720,height=480,framerate=30/1"
           << " ! omxh264enc name=" << encoderName
           << " target_bitrate=400000 control-rate=variable"
           << " ! video/x-h264, profile=(string)high, level=(string)4"
           << " ! rtph264pay config-interval=1 ! queue"
           << " ! udpsink host=" << host << " port=" << port
           << " sync=false name=" << sinkName;
  return buf;
}

static QString make_downlink(const QString &host, const QString &port,
                             debug_tag) {
  QString buf;
  QTextStream downlink(&buf);
  downlink << " ! queue "
           << " ! videoscale ! video/x-raw,width=720,height=480,framerate=30/1"
           << " ! rtpvrawpay ! queue"
           << " ! udpsink host=" << host << " port=" << port
           << " sync=false name=" << sinkName;
  return buf;

}

static QString make_filesink(int replica) {
  static constexpr char const *path_fmt = "/media/usb%1/camera/";
  static constexpr int width = 5;
  static constexpr char const *suffix_ = ".h264";
  auto path = QString(path_fmt).arg(replica);

  auto filenum = next_file(path, [](unsigned num) {
    QString fname;
    QTextStream fstream(&fname);
    fstream << qSetFieldWidth(width) << qSetPadChar(QChar('0')) << num
            << suffix_;
    return fname;
  });
  QString buf;
  QTextStream filesink(&buf);
  filesink << " ! queue ! multifilesink location=" << path << "%0" << width
           << "d" << suffix_ << " index=" << filenum << " next-file=3";
  return buf;
}

static QString make_pipeline(const int dev, const QString &host,
                             const QString &port, const bool debug) {
  QString teename("h264");
  QString buf;
  QTextStream pipeline(&buf);

  if (!debug) {
    pipeline << "uvch264src name=cam" << dev << " device=/dev/video" << dev
             << " initial-bitrate=5000000 peak-bitrate=5000000 "
                "average-bitrate=3000000"
             << " mode=mode-video rate-control=vbr auto-start=true"
             << " iframe-period=10000 cam0.vidsrc ! h264parse";
  } else {
    pipeline << "videotestsrc name=cam" << dev;
  }

  pipeline << " ! queue ! tee name=" << teename << " " << teename << ".";

  pipeline << (debug ? make_downlink(host, port, debug_tag{})
                     : make_downlink(host, port));

  if (!debug) {
    try {
      QString fsink = make_filesink(0);
      pipeline << " " << teename << "." << fsink;
    } catch (const std::exception &e) {
      qDebug() << "Skipping multifilesink: "
               << QString::fromStdString(e.what());
    }

    try {
      QString fsink = make_filesink(1);
      pipeline << " " << teename << "." << fsink;
    } catch (const std::exception &e) {
      qDebug() << "Skipping multifilesink: "
               << QString::fromStdString(e.what());
    }
  }

  qDebug() << buf;

  return buf;
}

struct VideoStreamSourceControl::Impl {
  QGst::PipelinePtr pipeline;

  Impl(const int dev, const QString &host, const QString &port,
       unsigned bitrate, const bool debug)
      : pipeline(QGst::Parse::launch(make_pipeline(dev, host, port, debug))
                     .dynamicCast<QGst::Pipeline>()) {
    pipeline->setState(QGst::StatePlaying);
  }
  ~Impl() { pipeline->setState(QGst::StateNull); }
};

VideoStreamSourceControl::VideoStreamSourceControl(const int dev,
                                                   const QString &host,
                                                   const QString &port,
                                                   unsigned bitrate, bool debug)
    : d(std::make_unique<Impl>(dev, host, port, bitrate, debug)) {
}

VideoStreamSourceControl::~VideoStreamSourceControl() = default;

QGst::ElementPtr VideoStreamSourceControl::getElementByName(const char *name) {
  auto elem = d->pipeline->getElementByName(name);

  if (!elem) {
    std::ostringstream os;
    os << "Pipeline " << d->pipeline->name().toStdString() << " element '"
       << name << "' not found.";
    throw std::runtime_error(os.str());
  }

  return elem;
}

void VideoStreamSourceControl::setBitrate(const uint64_t bitrate) {
  std::cout << "Setting bitrate: " << bitrate << std::endl;
  getElementByName(encoderName)->setProperty("target-bitrate", bitrate);
}

void VideoStreamSourceControl::setPort(const uint16_t port) {
  std::cout << "Setting port: " << port << std::endl;
  getElementByName(sinkName)->setProperty("port", static_cast<gint>(port));
}
