#pragma once

#include <memory>

namespace intex {
namespace hw {
namespace config {
struct gpio;
struct spi;
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

class MiniVNA {
  struct Impl;
  std::unique_ptr<Impl> d;

  MiniVNA(const config::gpio &config);

public:
  ~MiniVNA();
  void set(const bool on);

  static MiniVNA &miniVNA();
};

class USBHub {
  struct Impl;
  std::unique_ptr<Impl> d;

  USBHub(const config::gpio &config);

public:
  ~USBHub();
  void set(const bool on);

  static USBHub &usbHub();
};

class TemperatureSensor {
  struct Impl;
  std::unique_ptr<Impl> d;

  TemperatureSensor();

public:
  enum class Sensor : uint8_t { InnerRing, OuterRing, Atmosphere };
  double temperature(const enum Sensor sensor);
  static TemperatureSensor &temperatureSensor();
};

class ADS1248 {

  struct Impl;
  Impl *d;

public:
  ADS1248(const config::spi &config, const config::gpio &reset);
  double selftest(uint8_t sensor_select);
  static ADS1248 &sensor();
};

class spi;

class PressureSensor {
  struct Impl;
  std::unique_ptr<Impl> d;

  PressureSensor(spi &bus, const config::spi &config, const bool high = false);

public:
  double pressure();
  double temperature();

  static PressureSensor &atmosphere();
  static PressureSensor &antenna();
  static PressureSensor &tank();
};
}
}
