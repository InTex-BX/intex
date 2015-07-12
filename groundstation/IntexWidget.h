#pragma once

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

  void setPressure(const double pressure);

public Q_SLOTS:
  void setConnected(bool connected);
  void log(QString text);

  // clang-format off
Q_SIGNALS:
  void onConnectionChanged(bool connected);
  // clang-format on
  void depressurizeChanged(bool depressurize);
  void inflateChanged(bool inflate);
  void valve1Changed(bool open);
  void valve2Changed(bool open);
};
