#pragma once

#include <memory>

namespace intex {
namespace hw {
namespace config {
struct gpio;
}

class Valve {
  struct Impl;
  std::unique_ptr<Impl> d;

  Valve(const config::gpio &config);

public:
  ~Valve();

  void set(const bool state);
  static Valve &pressureTankValve();
  static Valve &outletValve();
};

class Heater {
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

class Burnwire {
  struct Impl;
  std::unique_ptr<Impl> d;

  Burnwire(const config::gpio &config);

public:
  ~Burnwire();

  void set(const bool on);

  static Burnwire &burnwire();
};

class Watchdog {
  struct Impl;
  std::unique_ptr<Impl> d;

  Watchdog(const config::gpio &config);

public:
  ~Watchdog();

  static Watchdog &watchdog();
};
}
}
