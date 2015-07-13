#pragma once

#include <string>

#include "qgst.h"

class VideoStreamSourceControl {
  QGst::PipelinePtr pipeline;

  QGst::ElementPtr getElementByName(const char *name);

public:
  VideoStreamSourceControl(std::string device, std::string host,
                           std::string port, unsigned bitrate = 400000);
  ~VideoStreamSourceControl();
  void setBitrate(const uint64_t bitrate);
  void setPort(const uint16_t port);
};
