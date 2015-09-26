#pragma once

#include <memory>

#include <QMainWindow>
#include <QString>

class Control : public QMainWindow {
  Q_OBJECT

  struct Impl;
  std::unique_ptr<Impl> d_;

public:
  explicit Control(QString host, const uint16_t port, const bool debug,
                   QWidget *parent = nullptr);
  ~Control();

private Q_SLOTS:
  void onConnect();
  void onDisconnect();
  void switchWidgets();
  void switchWindows();
  void showVideoControls(bool show);

private:
  void setupPipelines();
};

