#include <QDebug>
#include <QTimer>
#include <QString>
#include <QFileInfo>

#include <chrono>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

#include "IntexHardware.h"

using namespace std::chrono;
using namespace std::literals::chrono_literals;

namespace intex {
namespace hw {

namespace config {

/*
 * GPIO14 BURNWIRE1_EN
 * GPIO15 AUX1_24V_CTRL BURNWIRE-NEW
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

static const char *to_string(const enum config::gpio::direction &direction) {
  switch (direction) {
  case config::gpio::direction::in:
    return "in";
  case config::gpio::direction::out:
    return "out";
  }
}

static constexpr gpio valve0{5, "VALVE1", gpio::direction::out, false};
static constexpr gpio valve1{6, "VALVE2", gpio::direction::out, false};
static constexpr gpio heater0{19, "Heater 0", gpio::direction::out, false};
static constexpr gpio heater1{26, "Heater 1", gpio::direction::out, false};
static constexpr gpio burnwire{15, "Burnwire", gpio::direction::out, false};
}

static constexpr int retries = 3;

class gpio {
public:
  enum class attribute { active_low, direction, edge, value };
  gpio(const config::gpio &config);
  gpio(const gpio &) = delete;
  gpio(gpio &&) = default;
  gpio &operator=(const gpio &) = delete;
  gpio &operator=(gpio &&) = default;

  void set(const bool on);
  bool isOn();

private:
  config::gpio config_;
};

static std::ostream &operator<<(std::ostream &os,
                                const enum config::gpio::direction &direction) {
  return os << to_string(direction);
}

static QDebug &operator<<(QDebug &os,
                          const enum config::gpio::direction &direction) {
  return os << to_string(direction);
}

static std::ostream &operator<<(std::ostream &os,
                                const gpio::attribute attribute) {
  switch (attribute) {
  case gpio::attribute::active_low:
    return os << "active_low";
  case gpio::attribute::direction:
    return os << "direction";
  case gpio::attribute::edge:
    return os << "edge";
  case gpio::attribute::value:
    return os << "value";
  }
}

static void export_pin(int pin) {
  QFileInfo gpiodir(QString("/sys/class/gpio/gpio%1").arg(pin));
  if (gpiodir.exists())
    return;

  std::ofstream export_;

  export_.exceptions(std::ofstream::failbit | std::ofstream::badbit);

  export_.open("/sys/class/gpio/export");
  export_ << pin << std::endl;
}

static void sysfs_file(std::fstream &file, const gpio::attribute attr,
                       const int pin, const std::ios_base::openmode mode) {
  file.exceptions(std::ifstream::failbit | std::ifstream::badbit);

  std::ostringstream fname;
  fname << "/sys/class/gpio/gpio" << pin << "/" << attr;

  file.open(fname.str().c_str(), mode);
}

template <typename T>
static void set_attribute(const gpio::attribute attr, const int pin,
                          const T value) {
  using std::to_string;
  std::fstream file;
  sysfs_file(file, attr, pin, std::ios_base::out);
  file << to_string(value) << std::endl;
}

template <typename T>
static int get_attribute(const gpio::attribute attr, const int pin) {
  T value;
  std::fstream file;
  sysfs_file(file, attr, pin, std::ios_base::in);
  file >> value;
  return value;
}

gpio::gpio(const config::gpio &config) : config_(config) {
  std::cout << "Configuring GPIO " << config_.name << " (" << config_.pinno
            << ") as " << config_.direction
            << (config_.active_low ? "(active low)" : "") << "." << std::endl;

  export_pin(config_.pinno);
  set_attribute(attribute::active_low, config_.pinno, config_.active_low);
  set_attribute(attribute::direction, config_.pinno, config_.direction);
}

bool gpio::isOn() {
  return get_attribute<int>(attribute::value, config_.pinno);
}

void gpio::set(const bool on) {
  for (int retry = 0; retry < retries; ++retry) {
    set_attribute(attribute::value, config_.pinno, static_cast<int>(on));
    if (isOn() == on)
      return;
  }

  throw std::runtime_error("Could not set pin");
}

class debug_gpio {
public:
  debug_gpio(const config::gpio &config)
      : name_(config.name), pin_(config.pinno), direction_(config.direction),
        active_low_(config.active_low), state(false) {
    qDebug() << "Initializing pin" << name_ << "(" << pin_ << ") as"
             << direction_ << (active_low_ ? "(active_low)" : "");
  }
  debug_gpio(const debug_gpio &) = delete;
  debug_gpio(debug_gpio &&) = default;
  debug_gpio &operator=(const debug_gpio &) = delete;
  debug_gpio &operator=(debug_gpio &&) = default;

  void set(const bool on) {
    state = on;
    qDebug() << "Setting pin" << name_ << "(" << pin_ << ")" << state;
  }
  bool isOn() {
    qDebug() << "Reading pin" << name_ << "(" << pin_ << ") as" << state;
    return state;
  }

private:
  QString name_;
  int pin_;
  enum config::gpio::direction direction_;
  bool active_low_;
  bool state;
};

#pragma clang diagnostic ignored "-Wweak-vtables"

class PWM : public QObject {
  Q_OBJECT

  milliseconds period_;
  float duty_;
  QTimer timer;
  bool state_;

  void cycle() {
    Q_EMIT set(state_);
    const auto factor = state_ ? duty_ : 1.0 - duty_;
    const auto timeout = period_ * factor;
    state_ = !state_;
    timer.setInterval(static_cast<int>(timeout.count()));
  }

public:
  template <class Rep, class Period>
  PWM(duration<Rep, Period> period, float duty)
      : period_(duration_cast<milliseconds>(period)), duty_(duty),
        state_(true) {
    connect(&timer, &QTimer::timeout, this, &PWM::cycle);
  }

  void setDuty(float duty) { duty_ = duty; }
  void start() {
    if (!timer.isActive()) {
      Q_EMIT set(true);
      timer.start();
    }
  }
  void stop() {
    timer.stop();
    timer.setInterval(0);
    Q_EMIT set(false);
  }

  // clang-format off
Q_SIGNALS:
  void set(const bool);
  // clang-format on
};

class GPIO : public QObject {
  Q_OBJECT

public:
  template <typename T>
  GPIO(T &&backend)
     try : model_(std::make_unique<gpio_model<T>>(std::move(backend))) {
  } catch (const std::exception &e) {
    qCritical() << e.what();
  }
  void set(const bool on) {
    try {
      model_->set(on);
    } catch (const std::exception &e) {
      qCritical() << e.what();
    }
  }

private:
  struct gpio_concept {
    virtual ~gpio_concept() = default;
    virtual void set(const bool) = 0;
  };

  template <typename T> struct gpio_model final : gpio_concept {
    T backend_;
    gpio_model(T &&backend) : backend_(std::move(backend)) {}
    void set(const bool on) override { backend_.set(on); }
  };

  std::unique_ptr<gpio_concept> model_;
};

struct Valve::Impl {
  PWM pwm;
  GPIO pin_;
  bool enabled = false;
  QTimer timer;

  Impl(const config::gpio &config)
      : pwm(10s, 0.1f),
#ifdef BUILD_ON_RASPBERRY
        pin_(::intex::hw::gpio(config))
#else
        pin_(::intex::hw::debug_gpio(config))
#endif
  {
    timer.setInterval(duration_cast<milliseconds>(30s).count());
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, [this] { pwm.start(); });
    QObject::connect(&pwm, &PWM::set, &pin_, &GPIO::set);
  }

  void set(const bool on) {
    if (on == enabled) {
      return;
    }

    if (on) {
      timer.start();
    } else {
      timer.stop();
      pwm.stop();
    }

    pin_.set(on);
    enabled = on;
  }
};

Valve::Valve(const config::gpio &config) : d(std::make_unique<Impl>(config)) {}
Valve::~Valve() = default;
void Valve::set(const bool state) { d->set(state); }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wexit-time-destructors"
Valve &Valve::pressureTankValve() {
  static std::unique_ptr<Valve> instance{new Valve(intex::hw::config::valve0)};
  return *instance;
}

Valve &Valve::outletValve() {
  static std::unique_ptr<Valve> instance{new Valve(intex::hw::config::valve1)};
  return *instance;
}
#pragma clang diagnostic pop

class Heater::Impl {
  PWM pwm;
  GPIO pin;
  QTimer timer;
  int low_;
  int high_;
  milliseconds timeout_;

public:
  template <class Rep, class Period>
  Impl(const config::gpio &config, int low, int high,
       duration<Rep, Period> timeout)
      : pwm(2s, 1.0f),
#ifdef BUILD_ON_RASPBERRY
        pin(::intex::hw::gpio(config)),
#else
        pin(::intex::hw::debug_gpio(config)),
#endif
        low_(low), high_(high), timeout_(duration_cast<milliseconds>(timeout)) {
    QObject::connect(&pwm, &PWM::set, &pin, &GPIO::set);
    QObject::connect(&timer, &QTimer::timeout, [this]() { stop(); });
  }

  void start() {
    if (!timer.isActive()) {
      pwm.start();
      timer.start(static_cast<int>(timeout_.count()));
    }
  }
  void stop() {
    pwm.stop();
    timer.stop();
  }
  void temperatureChanged(int temperature) {
    if (temperature < low_) {
      qDebug() << "Low setpoint (" << low_ << ") reached (" << temperature
               << ").";
      start();
    } else if (temperature > high_)
      qDebug() << "High setpoint (" << high_ << ") reached (" << temperature
               << ").";
      stop();
  }
};

Heater::Heater(const config::gpio &config, int low, int high)
    : d(std::make_unique<Impl>(config, low, high, 10s)) {}
Heater::~Heater() = default;

void Heater::set(const bool state) {
  if (state)
    d->start();
  else
    d->stop();
}

void Heater::temperatureChanged(int temperature) {
  d->temperatureChanged(temperature);
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wexit-time-destructors"
Heater &Heater::innerHeater() {
  static std::unique_ptr<Heater> instance{
      new Heater(intex::hw::config::heater0, 5, 20)};
  return *instance;
}

Heater &Heater::outerHeater() {
  static std::unique_ptr<Heater> instance{
      new Heater(intex::hw::config::heater1, 5, 20)};
  return *instance;
}
#pragma clang diagnostic pop

struct Burnwire::Impl {
  GPIO pin;

  Impl(const config::gpio &config)
      :
#ifdef BUILD_ON_RASPBERRY
        pin(::intex::hw::gpio(config))
#else
        pin(::intex::hw::debug_gpio(config))
#endif
  {
  }

  void set(const bool on) {
    if (on) {
      QTimer::singleShot(duration_cast<milliseconds>(30s).count(),
                         [this] { pin.set(false); });
    }

    pin.set(on);
  }
};

Burnwire::Burnwire(const config::gpio &config)
    : d(std::make_unique<Impl>(config)) {}
Burnwire::~Burnwire() = default;
void Burnwire::set(const bool on) { d->set(on); }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wexit-time-destructors"
Burnwire &Burnwire::burnwire() {
  static std::unique_ptr<Burnwire> instance{
      new Burnwire(intex::hw::config::burnwire)};
  return *instance;
}
#pragma clang diagnostic pop
}
}
#pragma clang diagnostic ignored "-Wundefined-reinterpret-cast"
#include "IntexHardware.moc"
#include "moc_IntexHardware.cpp"
