#pragma once

#include <QFrame>

class IntexWidget : public QFrame {
public:
  IntexWidget(QWidget *parent = 0);
  ~IntexWidget();

  void setPressure(const double pressure);
};
