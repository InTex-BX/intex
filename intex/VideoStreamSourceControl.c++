#include <sstream>
#include <stdexcept>
#include <iostream>

#include <QDebug>

#include "VideoStreamSourceControl.h"

#ifdef INTEX_EXPERIMENT
static const char pipeline_fmt[] =
    "v4l2src device=/dev/video%1 name=src ! h264parse ! omxh264dec ! "
    "videoscale ! video/x-raw,width=768,height=432,framerate=30/1 ! "
    "omxh264enc name=encoder target_bitrate=%4 control-rate=variable ! "
    "rtph264pay config-interval=1 ! udpsink host=%1 port=%2 sync=false";
#else
static const char pipeline_fmt[] =
    "videotestsrc ! videoconvert ! rtpvrawpay ! "
    "queue ! "
    "udpsink host=%2 port=%3 sync=false";
#endif

static constexpr char encoderName[] = "encoder";
static constexpr char sinkName[] = "udpsink0";

VideoStreamSourceControl::VideoStreamSourceControl(std::string device,
                                                   std::string host,
                                                   std::string port,
                                                   unsigned bitrate)
    : pipeline(QGst::Parse::launch(QString(pipeline_fmt)
                                       .arg(QString::fromStdString(device),
                                            QString::fromStdString(host),
                                            QString::fromStdString(port),
                                            QString::number(bitrate)))
                   .dynamicCast<QGst::Pipeline>()) {
  qDebug() << QString(pipeline_fmt)
                  .arg(QString::fromStdString(device),
                       QString::fromStdString(host),
                       QString::fromStdString(port), QString::number(bitrate));
  //pipeline->setState(QGst::StatePlaying);
}

VideoStreamSourceControl::~VideoStreamSourceControl() {
  pipeline->setState(QGst::StateNull);
}

QGst::ElementPtr VideoStreamSourceControl::getElementByName(const char *name) {
  auto elem = pipeline->getElementByName(name);

  if (!elem) {
    std::ostringstream os;
    os << "Pipeline " << pipeline->name().toStdString() << " element '" << name
       << "' not found.";
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
