#pragma once

#include <QWidget>
#include <QSize>
#include <QFrame>
#include <QVBoxLayout>

#include "qgst.h"
#include "qgst_videowidget.h"

class VideoWidget : public QFrame {
  Q_OBJECT

  class Container;
  Container *container;

public:
  explicit VideoWidget(QWidget *parent = nullptr);
  void setVideoSink(const QGst::ElementPtr &sink);
  QGst::ElementPtr videoSink() const;
  void releaseVideoSink();

protected:
  void mouseDoubleClickEvent(QMouseEvent *event) Q_DECL_OVERRIDE;

Q_SIGNALS:
  void widgetClicked();
};
