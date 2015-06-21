#pragma once

#include <memory>

#include "qgst.h"
#include "qgst_videowidget.h"
#include "PipelineModifier.h"

#include "VideoWidget.h"

class SinkSwitcher;

class VideoStreamControl {
  QGst::PipelinePtr pipeline0;
  QGst::PipelinePtr pipeline1;

  enum class Type {
    Widget,
    Window,
  };

  enum class Side {
    Left,
    Right,
  };
  QGst::ElementPtr get(enum Type, enum Side);
  std::unique_ptr<SinkSwitcher> widgetSwitcher;
  std::unique_ptr<SinkSwitcher> windowSwitcher;

public:
  VideoStreamControl(VideoWidget &leftWidget, VideoWidget &rightWidget,
                     QGst::Ui::VideoWidget &leftWindow,
                     QGst::Ui::VideoWidget &rightWindow);
  ~VideoStreamControl();

  void switchWidgets();
  void switchWindows();
};

