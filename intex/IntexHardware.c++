#include <QDebug>
#include <QTimer>
#include <QString>

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

static constexpr int retries = 3;

class gpio {
public:
  enum class attribute { active_low, direction, edge, value };
  gpio(const config::gpio &config);
  gpio(const gpio &) = delete;
  gpio(gpio &&) = default;
  gpio &operator=(const gpio &) = delete;
  gpio &operator=(gpio &&) = default;

  void init();

  void on();
  void off();
  bool isOn();

private:
  config::gpio config_;
  void set(const bool on);
};

static const char *to_string(const enum config::gpio::direction &direction) {
  switch (direction) {
  case config::gpio::direction::in:
    return "in";
  case config::gpio::direction::out:
    return "out";
  }
}

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
  std::fstream file;
  sysfs_file(file, attr, pin, std::ios_base::out);
  file << value << std::endl;
}

template <typename T>
static int get_attribute(const gpio::attribute attr, const int pin) {
  T value;
  std::fstream file;
  sysfs_file(file, attr, pin, std::ios_base::in);
  file >> value;
  return value;
}

gpio::gpio(const config::gpio &config) : config_(config) {}

void gpio::init() {
  std::cout << "Configuring GPIO " << config_.name << " (" << config_.pinno
            << ") as " << config_.direction
            << (config_.active_low ? "(active low)" : "") << "." << std::endl;

  export_pin(config_.pinno);
  set_attribute(attribute::active_low, config_.pinno, config_.active_low);
  set_attribute(attribute::direction, config_.pinno, config_.direction);
}

void gpio::on() { set(true); }
void gpio::off() { set(false); }
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
        active_low_(config.active_low), state(false) {}
  debug_gpio(const debug_gpio &) = delete;
  debug_gpio(debug_gpio &&) = default;
  debug_gpio &operator=(const debug_gpio &) = delete;
  debug_gpio &operator=(debug_gpio &&) = default;

  void init() {
    qDebug() << "Initializing pin" << name_ << "(" << pin_ << ") as"
             << direction_ << (active_low_ ? "(active_low)" : "");
  }

  void on() { set(true); }
  void off() { set(false); }
  bool isOn() {
    qDebug() << "Reading pin" << name_ << "(" << pin_ << ") as" << state;
    return state;
  }

private:
  void set(const bool on) {
    state = on;
    qDebug() << "Setting pin" << name_ << "(" << pin_ << ")" << state;
  }
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
    if (state_) {
      Q_EMIT on();
    } else {
      Q_EMIT off();
    }
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
      Q_EMIT on();
      timer.start();
    }
  }
  void stop() {
    timer.stop();
    Q_EMIT off();
  }

  // clang-format off
Q_SIGNALS:
  void on();
  void off();
  // clang-format on
};

class GPIO : public QObject {
  Q_OBJECT

public:
  template <typename T>
  GPIO(T &&backend)
      : model_(std::make_unique<gpio_model<T>>(std::move(backend))) {}
  void init() {
    try {
      model_->init();
    } catch (const std::exception &e) {
      Q_EMIT log(QString::fromStdString(e.what()));
    }
  }
  void on() {
    try {
      model_->on();
    } catch (const std::exception &e) {
      Q_EMIT log(QString::fromStdString(e.what()));
    }
  }
  void off() {
    try {
      model_->off();
    } catch (const std::exception &e) {
      Q_EMIT log(QString::fromStdString(e.what()));
    }
  }

// clang-format off
Q_SIGNALS:
  void log(QString);
// clang-format on

private:
  struct gpio_concept {
    virtual ~gpio_concept() = default;
    virtual void init() = 0;
    virtual void on() = 0;
    virtual void off() = 0;
  };

  template <typename T> struct gpio_model final : gpio_concept {
    T backend_;
    gpio_model(T &&backend) : backend_(std::move(backend)) {}
    void init() override { backend_.init(); }
    void on() override { backend_.on(); }
    void off() override { backend_.off(); }
  };

  std::unique_ptr<gpio_concept> model_;
};

struct Valve::Impl {
  PWM pwm;
  GPIO pin_;
  Impl(const config::gpio &config)
      : pwm(2s, 0.1f),
#ifdef BUILD_ON_RASPBERRY
        pin_(::intex::hw::gpio(config))
#else
        pin_(::intex::hw::debug_gpio(config))
#endif
  {
    QObject::connect(&pwm, &PWM::on, &pin_, &GPIO::on);
    QObject::connect(&pwm, &PWM::off, &pin_, &GPIO::off);
  }
};

Valve::Valve(const config::gpio &config) : d(std::make_unique<Impl>(config)) {
  connect(&d->pin_, &GPIO::log, this, &Valve::log);
  d->pin_.init();
}
Valve::~Valve() = default;

void Valve::set(const bool state) {
  if (state)
    d->pwm.start();
  else
    d->pwm.stop();
}

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
      : pwm(2s, 0.1f),
#ifdef BUILD_ON_RASPBERRY
        pin(::intex::hw::gpio(config)),
#else
        pin(::intex::hw::debug_gpio(config)),
#endif
        low_(low), high_(high), timeout_(duration_cast<milliseconds>(timeout)) {
    QObject::connect(&pwm, &PWM::on, &pin, &GPIO::on);
    QObject::connect(&pwm, &PWM::off, &pin, &GPIO::off);
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
}
}
#pragma clang diagnostic ignored "-Wundefined-reinterpret-cast"
#include "IntexHardware.moc"
#include "moc_IntexHardware.cpp"
