#include <QDebug>
#include <QObject>
#include <QTimer>

#include <chrono>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

#include "IntexHardware.h"

using namespace std::chrono;

namespace intex {
namespace hw {

static constexpr int retries = 3;

class gpio {
public:
  enum class direction { in, out };
  enum class attribute { active_low, direction, edge, value };
  gpio(int pin, std::string name, const direction direction = direction::in,
       const bool active_low = false);
  gpio(const gpio &) = delete;
  gpio(gpio &&) = default;
  gpio &operator=(const gpio &) = delete;
  gpio &operator=(gpio &&) = default;

  void on();
  void off();
  bool isOn();

private:
  void set(const bool on);
  std::string name_;
  int pin_;
};

static std::ostream &operator<<(std::ostream &os,
                                const gpio::direction &direction) {
  switch (direction) {
  case gpio::direction::in:
    return os << "in";
  case gpio::direction::out:
    return os << "out";
  }
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
  std::ofstream export_("/sys/class/gpio/export");
  if (export_.fail())
    std::cout << "Error opening sysfs export file" << std::endl;

  export_ << pin << std::endl;
}

static std::fstream sysfs_file(const gpio::attribute attr, const int pin,
                               const std::ios_base::openmode mode) {
  std::fstream file;
  file.exceptions(std::ifstream::failbit | std::ifstream::badbit);

  std::ostringstream fname;
  fname << "/sys/class/gpio/gpio" << pin << "/" << attr;

  file.open(fname.str().c_str(), mode);
  return file;
}

template <typename T>
static void set_attribute(const gpio::attribute attr, const int pin,
                          const T value) {
  auto file = sysfs_file(attr, pin, std::ios_base::out);
  file << value << std::endl;
}

template <typename T>
static int get_attribute(const gpio::attribute attr, const int pin) {
  T value;
  auto file = sysfs_file(attr, pin, std::ios_base::in);
  file >> value;
  return value;
}

gpio::gpio(int pin, std::string name, const direction direction,
           const bool active_low)
    : name_(std::move(name)), pin_(pin) {
  std::cout << "Configuring GPIO " << name << " (" << pin << ") as "
            << direction << (active_low ? "(active low)" : "") << "."
            << std::endl;

  export_pin(pin_);
  set_attribute(attribute::active_low, pin_, active_low);
  set_attribute(attribute::direction, pin_, direction);
}

void gpio::on() { set(true); }
void gpio::off() { set(false); }
bool gpio::isOn() { return get_attribute<int>(attribute::value, pin_); }

void gpio::set(const bool on) {
  for (int retry = 0; retry < retries; ++retry) {
    set_attribute(attribute::value, pin_, static_cast<int>(on));
    if (isOn() == on)
      return;
  }

  throw std::runtime_error("Could not set pin");
}

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
    Q_EMIT on();
    timer.start();
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

struct Valve::Impl {
  PWM pwm;

  class GPIO : public QObject {
  public:
    template <typename T>
    GPIO(T &&backend)
        : model_(std::make_unique<gpio_model<T>>(std::move(backend))) {}
    void on() { model_->on(); }
    void off() { model_->off(); }

  private:
    struct gpio_concept {
      virtual ~gpio_concept() = default;
      virtual void on() = 0;
      virtual void off() = 0;
    };

    template <typename T> struct gpio_model final : gpio_concept {
      T backend_;
      gpio_model(T &&backend) : backend_(std::move(backend)) {}
      //~gpio_model = default;
      void on() override { backend_.on(); }
      void off() override { backend_.off(); }
    };

    std::unique_ptr<gpio_concept> model_;
  };

  GPIO pin_;
  Impl(const int pinno, std::string name)
      : pwm(2s, 0.1f), pin_(::intex::hw::gpio(pinno, std::move(name))) {
    QObject::connect(&pwm, &PWM::on, &pin_, &GPIO::on);
    QObject::connect(&pwm, &PWM::off, &pin_, &GPIO::off);
  }
};

Valve::Valve(const config::gpio &config)
    : d(std::make_unique<Impl>(config.pinno, config.name)) {}
Valve::~Valve() = default;

void Valve::set(const bool state) {
  if (state)
    d->pwm.start();
  else
    d->pwm.stop();
}
}
}
#pragma clang diagnostic ignored "-Wundefined-reinterpret-cast"
#include "IntexHardware.moc"
