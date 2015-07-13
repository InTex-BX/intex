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
  int pinno;
  const char *const name;
};

static constexpr gpio valve0{5, "VALVE1"};
static constexpr gpio valve1{6, "VALVE2"};
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
}
}
