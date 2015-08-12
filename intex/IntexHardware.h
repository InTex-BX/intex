#pragma once

#include <memory>

#include <QObject>
#include <QString>

namespace intex {
namespace hw {

namespace config {

/*
 * GPIO14 BURNWIRE1_EN
 * GPIO15 AUX1_24V_CTRL
 * GPIO18 ADS1248_CS0
 * GPIO23 ADS1248_CS1
 * GPIO24 RTC_CS
 * GPIO21 WD_INPUT
 * GPIO04 PRESSURE_CS0
 * GPIO17 PRESSURE_CS1
 * GPIO27 PRESSURE_CS2
 * GPIO10 SPI_MOSI
 * GPIO09 SPI_MISO
 * GPIO11 SPI_CLK
 * GPIO05 VALVE1_OPEN
 * GPIO06 VAVLE2_OPEN
 * GPIO13 VALVE3_OPEN
 * GPIO19 HEATER1_EN
 * GPIO26 HEATER2_EN
 */

struct gpio {
  enum class direction { in, out };

  int pinno;
  const char *const name;
  enum direction direction;
  bool active_low;
};

static constexpr gpio valve0{5, "VALVE1", gpio::direction::out, true};
static constexpr gpio valve1{6, "VALVE2", gpio::direction::out, true};
}

class Valve : public QObject {
  Q_OBJECT

  struct Impl;
  std::unique_ptr<Impl> d;

public:
  Valve(const config::gpio &config);
  ~Valve();

  void set(const bool state);

  // clang-format off
Q_SIGNALS:
  void log(QString msg);
  // clang-format on
};

class Heater : public QObject {
  Q_OBJECT

  class Impl;
  std::unique_ptr<Impl> d;

public:
  /* add timeout on temperatureChanged */
  Heater(const config::gpio &config, int low, int high);
  ~Heater();

  void set(const bool on);
  void temperatureChanged(int temperature);
};
}
}
