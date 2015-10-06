#pragma once

#include <functional>
#include <memory>

#include <QFrame>
#include <QObject>
#include <QString>

class IntexWidget : public QFrame {
  Q_OBJECT

  struct Impl;
  std::unique_ptr<Impl> d;

public:
  IntexWidget(QWidget *parent = 0);
  ~IntexWidget();

  void setTankPressure(const double pressure);
  void setAtmosphericPressure(const double pressure);
  void setAntennaPressure(const double pressure);
  void setAntennaInnerTemperature(const double temperature);
  void setAntennaOuterTemperature(const double temperature);
  void setAtmosphereTemperature(const double temperature);

public Q_SLOTS:
  void setConnected(bool connected);
  void log(QString text);
  void onValve1Changed(const bool state);
  void onValve2Changed(const bool state);

  // clang-format off
Q_SIGNALS:
  void onConnectionChanged(bool connected);
  // clang-format on
  void depressurizeRequest(bool depressurize,
                           std::function<void(bool)> success);
  void inflateRequest(bool inflate, std::function<void(bool)> success);
  void equalizeRequest(bool equalize, std::function<void(bool)> success);
  void valve1Request(bool open);
  void valve2Request(bool open);
};
