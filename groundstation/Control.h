#pragma once

#include <memory>

#include <QMainWindow>
#include <QString>

class Control : public QMainWindow {
  Q_OBJECT

  struct Impl;
  std::unique_ptr<Impl> d_;

public:
  explicit Control(QWidget *parent = nullptr);
  ~Control();

private Q_SLOTS:
  void onConnect();
  void onDisconnect();
  void switchWidgets();
  void switchWindows();
  void onBitrateChanged(int bitrate);
  void setPort0(const int port);
  void setPort1(const int port);
  void showVideoControls(bool show);

private:
  void setupPipelines();
};

