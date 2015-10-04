#pragma once

#include <memory>

#include <QString>

namespace intex {

struct telemetry {
  /* in °C */
  float cpu_temperature;
  float vna_tempature;
  float box_temperature;
  float inner_temperature;
  float outer_temperature;
  /* in mBar */
  float tank_pressure;
  float antenna_pressure;
  float atmospheric_pressure;
  /* states */
  bool inner_heater;
  bool outer_heater;
  bool tank_valve;
  bool outlet_valve;
  bool burnwire;
};

class ExperimentControl {
  class Impl;
  std::unique_ptr<Impl> d_;

public:
  ExperimentControl(QString host, quint16 port);
  ~ExperimentControl();
  ExperimentControl(const ExperimentControl &) = delete;
  ExperimentControl(ExperimentControl &&);
  ExperimentControl &operator=(const ExperimentControl &) = delete;
  ExperimentControl &operator=(ExperimentControl &&);

  void launched();
  void setTankValve(const bool on);
  void setOutletValve(const bool on);
  void setInnerHeater(const bool on);
  void setOuterHeater(const bool on);
  void setBurnwire(const bool on);
  void setUSBHub(const bool on);
  void setMiniVNA(const bool on);
  void measureAntenna();
  void videoStart(const InTexFeed service);
  void videoStop(const InTexFeed Sservice);
  void videoNext(const InTexFeed Sservice);
  void setVolume(const InTexFeed Sservice, float volume);
  void setBitrate(const InTexFeed service, uint64_t bitrate);
};
}
