#include <QDebug>
#include <QTimer>
#include <QString>
#include <QFileInfo>

#include <chrono>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

#include <cerrno>
#include <cstring>

#ifdef BUILD_ON_RASPBERRY
#include <linux/types.h>
extern "C" {
#include <linux/spi/spidev.h>
}
#endif

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

  friend QDebug &operator<<(QDebug &os, const gpio &config);
};

static const char *to_string(const enum config::gpio::direction &direction) {
  switch (direction) {
  case config::gpio::direction::in:
    return "in";
  case config::gpio::direction::out:
    return "out";
  }
}

static QDebug operator<<(QDebug os, const gpio &config) {
  os << "GPIO" << config.name << "\n";
  os << "  pin" << config.pinno << "\n";
  os << "  direction" << to_string(config.direction) << "\n";
  os << "  active" << (config.active_low ? "low" : "high");

  return os;
}

static constexpr gpio valve0{5, "Valve (outlet)", gpio::direction::out, false};
static constexpr gpio valve1{6, "Valve (tank)", gpio::direction::out, false};
static constexpr gpio heater0{19, "Heater (inner)", gpio::direction::out, false};
static constexpr gpio heater1{26, "Heater (outer)", gpio::direction::out, false};
static constexpr gpio burnwire{15, "Burnwire", gpio::direction::out, false};
static constexpr gpio watchdog{21, "Watchdog", gpio::direction::out, false};
static constexpr gpio mini_vna{20, "Mini VNA Supply", gpio::direction::out,
                               false};
static constexpr gpio usb_hub{24, "Hub supply", gpio::direction::out, false};

[[noreturn]] static void throw_errno(std::string what) {
  std::ostringstream os;
  os << what << " (" << errno << "): " << strerror(errno);
  throw std::runtime_error(os.str());
}

static constexpr int retries = 3;

class gpio {
  void configure();

public:
  enum class attribute { active_low, direction, edge, value };
  gpio(const config::gpio &config);
  gpio(const gpio &) = delete;
  gpio(gpio &&) = default;
  gpio &operator=(const gpio &) = delete;
  gpio &operator=(gpio &&) = default;

  void set(const bool on);
  bool isOn() const;

private:
  config::gpio config_;
};

static const char *to_string(const enum gpio::attribute &attribute) {
  switch (attribute) {
  case gpio::attribute::active_low:
    return "active_low";
  case gpio::attribute::direction:
    return "direction";
  case gpio::attribute::edge:
    return "edge";
  case gpio::attribute::value:
    return "value";
  }
}

static std::ostream &operator<<(std::ostream &os,
                                const enum config::gpio::direction &direction) {
  return os << to_string(direction);
}

static QDebug operator<<(QDebug os,
                         const enum config::gpio::direction &direction) {
  return os << to_string(direction);
}

static std::ostream &operator<<(std::ostream &os,
                                const gpio::attribute attribute) {
  return os << to_string(attribute);
}

static QDebug operator<<(QDebug os, const gpio::attribute attribute) {
  return os << to_string(attribute);
}

static void export_pin(int pin, const bool do_export = true) {
  QFileInfo gpiodir(QString("/sys/class/gpio/gpio%1").arg(pin));
  /* export but exists or unexport but doesn't exist */
  if (do_export == gpiodir.exists())
    return;

  std::ofstream export_;
  export_.clear();

  if (do_export) {
    export_.open("/sys/class/gpio/export");
  } else {
    export_.open("/sys/class/gpio/unexport");
  }
  export_ << pin << std::endl;
}

static void sysfs_file(std::fstream &file, const gpio::attribute attr,
                       const int pin, const std::ios_base::openmode mode) {
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
static T get_attribute(const gpio::attribute attr, const int pin) {
  T value;
  std::fstream file;
  sysfs_file(file, attr, pin, std::ios_base::in);
  file >> value;
  return value;
}

template <>
std::string get_attribute<std::string>(const gpio::attribute attr,
                                       const int pin) {
  char value[8] = {0};
  std::fstream file;
  sysfs_file(file, attr, pin, std::ios_base::in);
  file.getline(value, sizeof(value));
  return std::string(value);
}

void gpio::configure() {
  qDebug() << config_;
  export_pin(config_.pinno);
  set_attribute(attribute::direction, config_.pinno, config_.direction);
  set_attribute(attribute::active_low, config_.pinno, config_.active_low);
}

gpio::gpio(const config::gpio &config) : config_(config) { configure(); }

bool gpio::isOn() const {
  return get_attribute<int>(attribute::value, config_.pinno);
}

void gpio::set(const bool on) {
  for (int retry = 0; retry < retries; ++retry) {
    set_attribute(attribute::value, config_.pinno, static_cast<int>(on));
    if (isOn() == on)
      return;
    {
      export_pin(config_.pinno, false);
      configure();
    }
    std::this_thread::sleep_for(10ms);
  }

  throw_errno(QString("Could not set pin %1 %2")
                  .arg(config_.pinno)
                  .arg(on)
                  .toStdString());
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
  bool isOn() const {
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
  void set(const bool on) { model_->set(on); }
  bool state() const { return model_->state(); }

private:
  struct gpio_concept {
    virtual ~gpio_concept() = default;
    virtual void set(const bool) = 0;
    virtual bool state() const = 0;
  };

  template <typename T> struct gpio_model final : gpio_concept {
    T backend_;
    gpio_model(T &&backend) : backend_(std::move(backend)) {}
    void set(const bool on) override { backend_.set(on); }
    bool state() const override { return backend_.isOn(); }
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
  GPIO pin;
  QTimer timer;
  int low_;
  int high_;
  bool enabled = true;

  void setpin(const bool on) {
    /* don't restart timer, if it's running */
    if (!on && !timer.isActive()) {
      timer.start();
    } else if (on) {
      timer.stop();
    }
    pin.set(on);
  }

public:
  template <class Rep, class Period>
  Impl(const config::gpio &config, int low, int high,
       duration<Rep, Period> timeout)
      :
#ifdef BUILD_ON_RASPBERRY
        pin(::intex::hw::gpio(config)),
#else
        pin(::intex::hw::debug_gpio(config)),
#endif
        low_(low), high_(high) {
    timer.setInterval(
        static_cast<int>(duration_cast<milliseconds>(timeout).count()));
    QObject::connect(&timer, &QTimer::timeout, [this]() {
      qDebug() << "Temperature changed timeout reached. Resetting Heater.";
      setpin(true);
    });
  }

  void set(const bool on) {
    enabled = on;
    setpin(on);
  }

  void temperatureChanged(int temperature) {
    if (!enabled)
      return;

    if (temperature < low_) {
      qDebug() << "Low setpoint (" << low_ << ") reached (" << temperature
               << ").";
      setpin(true);
    } else if (temperature > high_) {
      qDebug() << "High setpoint (" << high_ << ") reached (" << temperature
               << ").";
      setpin(false);
    }
  }
};

Heater::Heater(const config::gpio &config, int low, int high)
    : d(std::make_unique<Impl>(config, low, high, 10s)) {}
Heater::~Heater() = default;
void Heater::set(const bool state) { d->set(state); }

void Heater::temperatureChanged(int temperature) {
  d->temperatureChanged(temperature);
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wexit-time-destructors"
Heater &Heater::innerHeater() {
  static std::unique_ptr<Heater> instance{
      new Heater(intex::hw::config::heater0, 35, 40)};
  return *instance;
}

Heater &Heater::outerHeater() {
  static std::unique_ptr<Heater> instance{
      new Heater(intex::hw::config::heater1, 35, 40)};
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

struct Watchdog::Impl {
  GPIO pin;
  QTimer timer;

  Impl(const config::gpio &config)
      :
#ifdef BUILD_ON_RASPBERRY
        pin(::intex::hw::gpio(config))
#else
        pin(::intex::hw::debug_gpio(config))
#endif
  {
    QObject::connect(&timer, &QTimer::timeout, [=] {
      try {
        pin.set(!pin.state());
      } catch (const std::exception &e) {
        qCritical() << e.what();
      }
    });
    timer.setInterval(duration_cast<milliseconds>(10s).count());
    timer.setSingleShot(false);
    timer.start();
  }
};

Watchdog::Watchdog(const config::gpio &config)
    : d(std::make_unique<Impl>(config)) {}
Watchdog::~Watchdog() = default;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wexit-time-destructors"
Watchdog &Watchdog::watchdog() {
  static std::unique_ptr<Watchdog> instance{
      new Watchdog(intex::hw::config::watchdog)};

  return *instance;
}
#pragma clang diagnostic pop

struct MiniVNA::Impl {
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

  void set(const bool on) { pin.set(on); }
};

MiniVNA::MiniVNA(const config::gpio &config)
    : d(std::make_unique<Impl>(config)) {}
MiniVNA::~MiniVNA() = default;

void MiniVNA::set(const bool on) { d->set(on); }
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wexit-time-destructors"
MiniVNA &MiniVNA::miniVNA() {
  static std::unique_ptr<MiniVNA> instance{
      new MiniVNA(intex::hw::config::mini_vna)};

  return *instance;
}
#pragma clang diagnostic pop

struct USBHub::Impl {
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

  void set(const bool on) { pin.set(on); }
};

USBHub::USBHub(const config::gpio &config)
    : d(std::make_unique<Impl>(config)) {}
USBHub::~USBHub() = default;

void USBHub::set(const bool on) { d->set(on); }
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wexit-time-destructors"
USBHub &USBHub::usbHub() {
  static std::unique_ptr<USBHub> instance{
      new USBHub(intex::hw::config::usb_hub)};

  return *instance;
}
#pragma clang diagnostic pop

}
}
#pragma clang diagnostic ignored "-Wundefined-reinterpret-cast"
#include "IntexHardware.moc"
#include "moc_IntexHardware.cpp"
