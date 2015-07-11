#pragma once

#include <QFrame>

class IntexWidget : public QFrame {
  Q_OBJECT

public:
  IntexWidget(QWidget *parent = 0);
  ~IntexWidget();

  void setPressure(const double pressure);

public Q_SLOTS:
  void setConnected(bool connected);

  // clang-format off
Q_SIGNALS:
  void onConnectionChanged(bool connected);
  // clang-format on
  void depressurizeChanged(bool depressurize);
  void inflateChanged(bool inflate);
  void valve1Changed(bool open);
  void valve2Changed(bool open);
};
