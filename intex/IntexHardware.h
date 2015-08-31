#pragma once

#include <memory>

#include <QObject>
#include <QString>

namespace intex {
namespace hw {
namespace config {
struct gpio;
}

class Valve : public QObject {
  Q_OBJECT

  struct Impl;
  std::unique_ptr<Impl> d;

  Valve(const config::gpio &config);

public:
  ~Valve();

  void set(const bool state);
  static Valve &pressureTankValve();
  static Valve &outletValve();
};

class Heater : public QObject {
  Q_OBJECT

  class Impl;
  std::unique_ptr<Impl> d;

  /* add timeout on temperatureChanged */
  Heater(const config::gpio &config, int low, int high);

public:
  ~Heater();

  void set(const bool on);
  void temperatureChanged(int temperature);

  static Heater &innerHeater();
  static Heater &outerHeater();
};
}
}
