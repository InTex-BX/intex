#include <QCoreApplication>
#include <QTimer>

#include "IntexHardware.h"

int main(int argc, char *argv[]) {
  QCoreApplication application(argc, argv);
  QTimer::singleShot(0, [] { intex::hw::Watchdog::watchdog(); });

  application.exec();
}
