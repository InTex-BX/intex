#pragma once

#include <QMainWindow>
#include <QPoint>
#include <QShortcut>

#include "qgst_videowidget.h"

class VideoWindow : public QMainWindow {
  Q_OBJECT

  QGst::Ui::VideoWidget *widget_;
  QPoint currentPos;
  QShortcut close;

public:
  explicit VideoWindow(QWidget *parent);
  QGst::Ui::VideoWidget *videoWidget() { return widget_; }

protected:
  bool eventFilter(QObject *obj, QEvent *ev);
  void mouseMoveEvent(QMouseEvent *event);
};
