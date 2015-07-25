#pragma once

#include <memory>
#include <string>

#include "qgst.h"
#include "intex.h"

#ifdef BUILD_ON_RASPBERRY
static constexpr bool debug_default() { return false; }
#else
static constexpr bool debug_default() { return true; }
#endif

class VideoStreamSourceControl {
  struct Impl;
  std::unique_ptr<Impl> d;

  QGst::ElementPtr getElementByName(const char *name);

public:
  VideoStreamSourceControl(const enum intex::Subsystem subsystem,
                           const QString &host, const QString &port,
                           unsigned bitrate = 400000,
                           bool debug = debug_default());
  ~VideoStreamSourceControl();
  void setBitrate(const uint64_t bitrate);
  void setPort(const uint16_t port);
};
