#include <chrono>
#include <atomic>
#include <string>
#include <functional>

#include <cmath>

#include <QObject>
#include <QFile>
#include <QTextStream>
#include <QUdpSocket>
#include <QDebug>
#include <QtGlobal>
#include <QtSerialPort/QtSerialPort>
#include <QTimerEvent>
#include <QAbstractSocket>
#include <QProcess>

#include <capnp/message.h>
#include <capnp/serialize.h>
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnon-virtual-dtor"
#pragma clang diagnostic ignored "-Wweak-vtables"
#include "rpc/intex.capnp.h"
#pragma clang diagnostic pop

#include "ExperimentControl.h"
#include "VideoStreamSourceControl.h"
#include "IntexHardware.h"
#include "intex.h"
#include "sysfs.h"

using namespace std::literals::chrono_literals;
using namespace std::chrono;

namespace intex {

static float cpu_temperature() {
  QFile file("/sys/class/thermal/thermal_zone0/temp");
  int temperature;

  if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    QTextStream in(&file);
    in >> temperature;

    return static_cast<float>(temperature) / 1000.0f;
  }

  throw std::runtime_error(
      qPrintable("Could not open file " + file.fileName()));
}

static float vna_temperature() {
  static constexpr qint32 baudrate = 921600;
  static const char read_temperature_command[] = "10\r";
  char buf[2];
  // TODO turn on VNA

  QSerialPort vna("/dev/ttyUSB0");
  if (!vna.open(QIODevice::ReadWrite)) {
    throw std::runtime_error(
        qPrintable("Could not open serial port " + vna.portName()));
  }

  if (!vna.setBaudRate(baudrate)) {
    throw std::runtime_error("Could not set baudrate to " +
                             std::to_string(baudrate));
  }

  if (!vna.setRequestToSend(true)) {
    throw std::runtime_error("Enabling request to send failed.");
  }

  if (vna.write(read_temperature_command) < 0) {
    throw std::runtime_error("Could not send read temperature command.");
  }

  for (size_t i = 0; i < sizeof(buf);) {
    if (vna.waitForReadyRead(20)) {
      auto ret = vna.read(&buf[i], static_cast<qint64>(sizeof(buf) - i));
      if (ret < 0) {
        throw std::runtime_error(qPrintable("Error reading " + vna.portName() +
                                            ": " + vna.errorString()));
      }
      i += static_cast<size_t>(ret);
    } else {
      throw std::runtime_error(
          qPrintable("Reading from " + vna.portName() + " timed out."));
    }
  }

  uint16_t tmp;
  memcpy(&tmp, buf, sizeof(buf));
  return static_cast<float>(tmp) / 10.0f;
}

static kj::Array<capnp::word> build_announce(const AutoAction action,
                                             const unsigned timeout) {
  ::capnp::MallocMessageBuilder message;
  auto request = message.initRoot<AutoActionRequest>();

  request.setAction(action);
  request.setTimeout(timeout);

  return messageToFlatArray(message);
}

class ValueServer {
  QUdpSocket socket;

public:
  ValueServer() = default;
  void connectToHost(const QString &host, uint16_t port) {
    socket.connectToHost(host, port, QIODevice::WriteOnly,
                         QAbstractSocket::IPv4Protocol);
  }
};

static constexpr auto open = true;
static constexpr auto closed = false;
static constexpr auto On = true;
static constexpr auto Off = false;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wweak-vtables"
class ExperimentControl::Impl : public QObject {
#ifdef BUILD_ON_RASPBERRY
#if 0
  static constexpr auto ascend_timeout = 1000s;
  static constexpr auto burnwire_timeout = 30s;
  static constexpr auto inflation_timeout = 570s;
  static constexpr auto cure_timeout = 600s;
  static constexpr auto equalization_timeout = 30s;
#else
  static constexpr auto ascend_timeout = 100s;
  static constexpr auto burnwire_timeout = 30s;
  static constexpr auto inflation_timeout = 57s;
  static constexpr auto cure_timeout = 60s;
  static constexpr auto equalization_timeout = 30s;
#endif
#else
  static constexpr auto ascend_timeout = 10s;
  static constexpr auto burnwire_timeout = 3s;
  static constexpr auto inflation_timeout = 5s;
  static constexpr auto cure_timeout = 6s;
  static constexpr auto equalization_timeout = 3s;
#endif

  /*
   * Heizung an (35,40 °C), fail-on
   * ascend: outlet 30s / 0.1f
   * floating: delta-p const 2min && p < 100mBar -> inflate; timer TBD
   *  - NWA measurement
   *  - burnwire on 30s
   *  - inflate: 30s (TBC), PWM refresh 10s/1s for 600s total (570s PWM)
   * - NWA measurement
   * - 600s curing
   * - equalization 30s open; 10s / 0.1f after that
   * - NWA measurement
   */

  enum class state : uint8_t {
    preflight,
    ascending,
    floating,
    measuring1,
    burnwire,
    inflating,
    measuring2,
    curing,
    equalizing,
    measuring3,
    descending,
  };

  friend QDebug &operator<<(QDebug &os, const enum state &state) {
    switch (state) {
    case state::preflight:
      return os << "Preflight";
    case state::ascending:
      return os << "Ascending";
    case state::floating:
      return os << "Floating";
    case state::measuring1:
      return os << "Measuring 1";
    case state::burnwire:
      return os << "Burnwire";
    case state::inflating:
      return os << "Inflating";
    case state::measuring2:
      return os << "Measuring 2";
    case state::curing:
      return os << "Curing";
    case state::equalizing:
      return os << "Equalizing";
    case state::measuring3:
      return os << "Measuring 3";
    case state::descending:
      return os << "Descending";
    }
  }

  QString host;
  quint16 port;
  enum state flight_state;
  QTimer timeout;
  int heartbeat_id;
  QUdpSocket telemetry_socket;
  QUdpSocket announce_socket;
  bool announce_reply_outstanding = false;
  std::function<void(void)> auto_callback;
  QTimer telemetry_timer;
  QString telemetry_filename;
  QFile telemetry_file;
  QProcess nva;

  std::unique_ptr<VideoStreamSourceControl> source0;
  std::unique_ptr<VideoStreamSourceControl> source1;

  static bool nvramFile(QFile &file) {
#ifdef BUILD_ON_RASBERRY
    QDir nvram;
    return false;
#else
    QDir nvram("/Volumes/Intex");

    if (!nvram.exists())
      return false;

    file.setFileName(nvram.filePath("nvram"));
    if (!file.open(QIODevice::ReadWrite))
      return false;
#endif
    return true;
  }

  static enum state loadState() {
    QFile file;
    if (!nvramFile(file))
      return state::preflight;

    auto buf = file.readAll();
    intex::QByteArrayMessageReader reader(buf);
    auto s = reader.getRoot<State>();

    switch (s.getState()) {
    case static_cast<uint8_t>(state::preflight):
      return state::preflight;
    case static_cast<uint8_t>(state::ascending):
      return state::ascending;
    case static_cast<uint8_t>(state::floating):
      return state::floating;
    case static_cast<uint8_t>(state::measuring1):
      return state::measuring1;
    case static_cast<uint8_t>(state::burnwire):
      return state::burnwire;
    case static_cast<uint8_t>(state::inflating):
      return state::inflating;
    case static_cast<uint8_t>(state::measuring2):
      return state::measuring2;
    case static_cast<uint8_t>(state::curing):
      return state::curing;
    case static_cast<uint8_t>(state::equalizing):
      return state::equalizing;
    case static_cast<uint8_t>(state::measuring3):
      return state::measuring3;
    case static_cast<uint8_t>(state::descending):
      return state::descending;
    }
    return state::preflight;
  }

  void saveState(const enum state state) {
    /* save state and time of state finish, so that on re-start a timer can
     * be set appropriately */
    ::capnp::MallocMessageBuilder message;
    auto save = message.initRoot<State>();

    save.setState(static_cast<uint8_t>(state));
    auto flat = messageToFlatArray(message);
    auto chars = flat.asChars();

    QFile file;
    if (!nvramFile(file))
      return;
    file.write(chars.begin(), static_cast<qint64>(chars.size()));
  }

  void timerEvent(QTimerEvent *event) Q_DECL_OVERRIDE {
    if (event->timerId() == heartbeat_id) {
      run();
    }
  }

  void send_telemetry(kj::ArrayPtr<char> &buffer) {
    auto ret = telemetry_socket.write(buffer.begin(),
                                      static_cast<qint64>(buffer.size()));
    if (ret < 0) {
      qCritical() << "Sending telemetry datagram failed:"
                  << telemetry_socket.error() << "Reconnecting.";
      telemetry_socket.connectToHost(host, port);
    } else {
#if 0
      qDebug().nospace() << "Telemetry datagram of size " << buffer.size()
                         << " (" << ret << ") sent successfully.";
#endif
    }
  }

  void save_telemetry(kj::ArrayPtr<char> &buffer) {
    auto ret = telemetry_file.write(buffer.begin(),
                                    static_cast<qint64>(buffer.size()));

    if (ret < 0) {
      qCritical() << "Writing telemetry datagram failed:"
                  << telemetry_file.error() << "Opening new file.";
      telemetry_filename = storageLocation(Subsystem::Telemetry);
      telemetry_file.setFileName(telemetry_filename);
      telemetry_file.open(QIODevice::WriteOnly);
    } else {
#if 0
      qDebug().nospace() << "Telemetry datagram of size " << buffer.size()
                         << " (" << ret << ") saved successfully.";
#endif
    }

    telemetry_file.flush();
  }

  void handle_auto_timeout() {
    if (announce_reply_outstanding) {
      announce_reply_outstanding = false;
      qDebug() << "Auto request timed out";
      auto_callback();
    }
  }

  void handle_auto_datagram(QByteArray &buffer) {
    auto reader = intex::QByteArrayMessageReader(buffer);
    auto reply = reader.getRoot<AutoActionReply>();

    qDebug() << "Result:" << to_string(reply.getAction()) << reply.isAccept()
             << reply.isCancel();
    if (reply.isAccept()) {
      handle_auto_timeout();
    } else {
      qDebug() << "Action cancelled";
    }
  }

  void announceAction(const AutoAction action,
                      const std::chrono::seconds announce_timeout,
                      std::function<void(void)> ok_action) {
    if (announce_socket.state() == QAbstractSocket::ConnectedState) {
      auto data = build_announce(
          action, static_cast<unsigned>(announce_timeout.count()));
      auto chars = data.asChars();
      auto ret = announce_socket.write(chars.begin(),
                                       static_cast<qint64>(chars.size()));

      if (ret < 0) {
        qCritical() << "Sending announce datagram failed:"
                    << announce_socket.error() << "Reconnecting.";
        announce_socket.connectToHost(host, intex_auto_request_port());
      }
    } else if (announce_socket.state() == QAbstractSocket::UnconnectedState) {
      announce_socket.connectToHost(host, intex_auto_request_port());
    }

    announce_reply_outstanding = true;
    auto_callback = ok_action;
    QTimer::singleShot(
        static_cast<int>(duration_cast<milliseconds>(announce_timeout).count()),
        [this] { handle_auto_timeout(); });
  }

  void build_telemetry(::capnp::MallocMessageBuilder &message) {
    Telemetry::Builder telemetry = message.initRoot<Telemetry>();

    auto cpu_temp = telemetry.initCpuTemperature();
    cpu_temp.setTimestamp(system_clock::now().time_since_epoch().count());
    try {
      cpu_temp.initReading().setValue(cpu_temperature());
    } catch (const std::runtime_error &e) {
      cpu_temp.initError().setReason(e.what());
    }

    auto vna_temp = telemetry.initVnaTemperature();
    vna_temp.setTimestamp(system_clock::now().time_since_epoch().count());
    try {
      if (nva.state() != QProcess::ProcessState::NotRunning)
        throw std::runtime_error("NVA measurement running");
      vna_temp.initReading().setValue(vna_temperature());
    } catch (const std::runtime_error &e) {
      vna_temp.initError().setReason(e.what());
    }

    try {
#if 0
    qDebug() << "InnerRing:"
             << hw::TemperatureSensor::temperatureSensor().temperature(
                    hw::TemperatureSensor::Sensor::InnerRing);
    qDebug() << "OuterRing:"
             << hw::TemperatureSensor::temperatureSensor().temperature(
                    hw::TemperatureSensor::Sensor::OuterRing);
    qDebug() << "Atmosphere:"
             << hw::TemperatureSensor::temperatureSensor().temperature(
                    hw::TemperatureSensor::Sensor::Atmosphere);
#else
// hw::ADS1248::sensor().selftest(1);
#endif
    } catch (const std::runtime_error &e) {
      qCritical() << e.what();
    }
  }

  void enableCameras() {
    QTime duration;

    for (duration.start();
         duration.elapsed() < duration_cast<milliseconds>(1s).count();) {
      try {
        findDevice(1);
      } catch (const std::runtime_error &e) {
        qCritical() << e.what();
        continue;
      }
      break;
    }
  }

  template <typename Callback>
  auto dispatch_video_controls(const InTexFeed service, Callback &&callback) {
    switch (service) {
    case InTexFeed::FEED0:
      if (source0)
        return callback(source0);
      else
        throw std::runtime_error("Cam 0 not initialized");
    case InTexFeed::FEED1:
      if (source1)
        return callback(source1);
      else
        throw std::runtime_error("Cam 1 not initialized");
    }
  }

public:
  Impl(QString host_, quint16 port_)
      : host(std::move(host_)), port(port_), flight_state(loadState()),
        heartbeat_id(startTimer((1000ms).count())),
        telemetry_filename(storageLocation(Subsystem::Telemetry)),
        telemetry_file(telemetry_filename) {
    setUSBHub(On);
    setMiniVNA(Off);
    setBurnwire(Off);
    setInnerHeater(On);
    setInnerHeater(On);
    setTankValve(Off);
    setOutletValve(Off);
    intex::hw::Watchdog::watchdog();
    telemetry_file.open(QIODevice::WriteOnly);
    telemetry_timer.setInterval(duration_cast<milliseconds>(5s).count());
    telemetry_timer.setSingleShot(false);
    connect(&telemetry_timer, &QTimer::timeout, [this] {
      ::capnp::MallocMessageBuilder message;
      build_telemetry(message);
      auto data = messageToFlatArray(message);
      auto chars = data.asChars();
      send_telemetry(chars);
      save_telemetry(chars);
    });
    connect(&telemetry_socket, &QAbstractSocket::connected, [this] {
      qDebug().nospace() << "Sending telemetry data to "
                         << telemetry_socket.peerName() << ":"
                         << telemetry_socket.peerPort();
      telemetry_timer.start();
    });

    telemetry_socket.connectToHost(host, port);
    if (heartbeat_id == 0) {
      qDebug() << "Could not allocate heartbeat timer. Automatic experiment "
                  "control disabled.";
    }

    connect(&announce_socket, &QAbstractSocket::readyRead, [this] {
      intex::handle_datagram(announce_socket,
                             [this](auto &&buffer, QHostAddress &, quint16) {
                               handle_auto_datagram(buffer);
                             });
    });
    intex::bind_socket(&announce_socket, intex_auto_reply_port(),
                       "Announce Reply");
    announce_socket.connectToHost(host, intex_auto_request_port());

    enableCameras();
    source0 = std::make_unique<VideoStreamSourceControl>(
        intex::Subsystem::Video0, host, 5000);
    source1 = std::make_unique<VideoStreamSourceControl>(
        intex::Subsystem::Video1, host, 5010);
  }
  ~Impl() noexcept {}

  void launched() { change_state(state::ascending); }
  void ascending_timedout() { change_state(state::floating); }
  void burnwire_timedout() { change_state(state::inflating); }
  void inflating_timedout() { change_state(state::measuring2); }
  void curing_timedout() { change_state(state::equalizing); }
  void equalization_timedout() { change_state(state::measuring3); }

  void change_state(enum state next_state);
  void run();

  void setTankValve(const bool on) {
    intex::hw::Valve::pressureTankValve().set(on);
  }
  void setOutletValve(const bool on) {
    intex::hw::Valve::outletValve().set(on);
  }
  void setInnerHeater(const bool on) {
    intex::hw::Heater::innerHeater().set(on);
  }
  void setOuterHeater(const bool on) {
    intex::hw::Heater::outerHeater().set(on);
  }
  void setBurnwire(const bool on) { intex::hw::Burnwire::burnwire().set(on); }
  void setUSBHub(const bool on) { intex::hw::USBHub::usbHub().set(on); }
  void setMiniVNA(const bool on) { intex::hw::MiniVNA::miniVNA().set(on); }

  void videoStart(const InTexFeed feed) {
    dispatch_video_controls(feed, [](auto &&source) { source->start(); });
  }

  void videoStop(const InTexFeed feed) {
    dispatch_video_controls(feed, [](auto &&source) { source->stop(); });
  }

  void videoNext(const InTexFeed feed) {
    dispatch_video_controls(feed, [](auto &&source) { source->next(); });
  }

  void setVolume(const InTexFeed feed, const float volume) {
    dispatch_video_controls(
        feed, [volume](auto &&source) { source->setVolume(volume); });
  }

  void setBitrate(const InTexFeed feed, const uint64_t bitrate) {
    dispatch_video_controls(
        feed, [bitrate](auto &&source) { source->setBitrate(bitrate); });
  }

  void start_measurement() {
    if (nva.state() != QProcess::ProcessState::NotRunning) {
      qCritical() << "VNA measurement already running";
      return;
    }
    intex::hw::MiniVNA::miniVNA().set(On);
    nva.setProcessChannelMode(QProcess::MergedChannels);
    nva.setProgram("java");
    QStringList args;
    args << "-Dfstart=370000000";
    args << "-Dfstop=500000000";
    args << "-Dfsteps=261";
    args << "-Dcalfile=REFL_tinyVNA-2015-10-04.cal";
    args << "-Dscanmode=REFL";
    args << "-Dexports=snp";
    args << "-jar";
    args << "/home/intex/vnaJ-hl.3.1.5.jar";
    nva.setArguments(args);

    connect(
        &nva, static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(
                  &QProcess::finished),
        [this](const int exit_code, const QProcess::ExitStatus exit_status) {
          qDebug() << "Measurement done" << exit_code << exit_status << ":";
          qDebug() << nva.readAllStandardOutput();
          intex::hw::MiniVNA::miniVNA().set(Off);
        });
    nva.start();
  }
};

void ExperimentControl::Impl::change_state(enum state next_state) {
  timeout.stop();
  disconnect(&timeout, &QTimer::timeout, this, nullptr);

  qDebug() << "Entering state" << next_state;

  switch (next_state) {
  case state::preflight:
    break;
  case state::ascending:
    setOutletValve(open);
    connect(&timeout, &QTimer::timeout, this, &Impl::ascending_timedout);
    timeout.start(duration_cast<milliseconds>(ascend_timeout).count());
    break;
  case state::floating:
    setOutletValve(open);
    announceAction(AutoAction::INFLATE, 30s,
                   [this] { change_state(state::burnwire); });
    break;
  case state::measuring1:
    break;
  case state::burnwire:
    setBurnwire(On);
    connect(&timeout, &QTimer::timeout, this, &Impl::burnwire_timedout);
    timeout.start(duration_cast<milliseconds>(burnwire_timeout).count());
    break;
  case state::inflating:
    setBurnwire(Off);
    setOutletValve(closed);
    setTankValve(open);
    connect(&timeout, &QTimer::timeout, this, &Impl::inflating_timedout);
    timeout.start(duration_cast<milliseconds>(inflation_timeout).count());
    break;
  case state::measuring2:
    setOutletValve(closed);
    setTankValve(open);
    QTimer::singleShot(0, [this] { change_state(state::curing); });
    break;
  case state::curing:
    connect(&timeout, &QTimer::timeout, this, &Impl::curing_timedout);
    timeout.start(duration_cast<milliseconds>(cure_timeout).count());
    announceAction(AutoAction::DEFLATE, 30s,
                   [this] { change_state(state::equalizing); });
    break;
  case state::equalizing:
    setTankValve(closed);
    setOutletValve(open);
    connect(&timeout, &QTimer::timeout, this, &Impl::equalization_timedout);
    timeout.start(duration_cast<milliseconds>(equalization_timeout).count());
    break;
  case state::measuring3:
    setTankValve(closed);
    setOutletValve(open);
    QTimer::singleShot(0, [this] { change_state(state::descending); });
    break;
  case state::descending:
    break;
  }

  if (next_state != flight_state) {
    flight_state = next_state;
    saveState(flight_state);
  }
}

void ExperimentControl::Impl::run() {
  // qDebug() << "Run state" << flight_state;

  switch (flight_state) {
  case state::preflight:
    break;
  case state::ascending:
    break;
  case state::floating:
    break;
  case state::measuring1:
    break;
  case state::burnwire:
    break;
  case state::inflating:
    break;
  case state::measuring2:
    break;
  case state::curing:
    break;
  case state::equalizing:
    break;
  case state::measuring3:
    break;
  case state::descending:
    break;
  }
}

#pragma clang diagnostic pop
constexpr seconds ExperimentControl::Impl::ascend_timeout;
constexpr seconds ExperimentControl::Impl::burnwire_timeout;
constexpr seconds ExperimentControl::Impl::inflation_timeout;
constexpr seconds ExperimentControl::Impl::cure_timeout;
constexpr seconds ExperimentControl::Impl::equalization_timeout;

ExperimentControl::ExperimentControl(QString host, quint16 port)
    : d_(std::make_unique<Impl>(std::move(host), port)) {}
ExperimentControl::~ExperimentControl() = default;
ExperimentControl::ExperimentControl(ExperimentControl &&) = default;
ExperimentControl &ExperimentControl::operator=(ExperimentControl &&) = default;

void ExperimentControl::launched() { d_->launched(); }
void ExperimentControl::setTankValve(const bool on) { d_->setTankValve(on); }
void ExperimentControl::setOutletValve(const bool on) {
  d_->setOutletValve(on);
}
void ExperimentControl::setInnerHeater(const bool on) {
  d_->setInnerHeater(on);
}
void ExperimentControl::setOuterHeater(const bool on) {
  d_->setOuterHeater(on);
}
void ExperimentControl::setBurnwire(const bool on) { d_->setBurnwire(on); }

void ExperimentControl::setUSBHub(const bool on) { d_->setUSBHub(on); }
void ExperimentControl::setMiniVNA(const bool on) { d_->setMiniVNA(on); }

void ExperimentControl::videoStart(const InTexFeed feed) {
  d_->videoStart(feed);
}
void ExperimentControl::videoStop(const InTexFeed feed) { d_->videoStop(feed); }
void ExperimentControl::videoNext(const InTexFeed feed) { d_->videoNext(feed); }
void ExperimentControl::setVolume(const InTexFeed feed, const float volume) {
  d_->setVolume(feed, volume);
}
void ExperimentControl::setBitrate(const InTexFeed feed,
                                   const uint64_t bitrate) {
  d_->setBitrate(feed, bitrate);
}

void ExperimentControl::measureAntenna() { d_->start_measurement(); };
}

#include "ExperimentControl.moc"
