#pragma once

#include <memory>

#include "qgst.h"
#include "qgst_videowidget.h"
#include "PipelineModifier.h"

#include "VideoWidget.h"

class SinkSwitcher;

class VideoStreamControl {
public:
  enum class Stream {
    Left,
    Right,
  };

private:
  QGst::PipelinePtr pipeline0;
  QGst::PipelinePtr pipeline1;

  enum class Type {
    Widget,
    Window,
  };

  QGst::ElementPtr get(enum Type, enum Stream);
  QGst::PipelinePtr get(enum Stream);
  std::unique_ptr<SinkSwitcher> widgetSwitcher;
  std::unique_ptr<SinkSwitcher> windowSwitcher;

public:
  VideoStreamControl(VideoWidget &leftWidget, VideoWidget &rightWidget,
                     QGst::Ui::VideoWidget &leftWindow,
                     QGst::Ui::VideoWidget &rightWindow);
  ~VideoStreamControl();

  void switchWidgets();
  void switchWindows();
  void setPort(const enum Stream side, const int port);
  void setAddress(const QString &address);
};

