#include <QApplication>

#include "qgst.h"
#include "Control.h"

int main(int argc, char *argv[]) {
  QCoreApplication::setOrganizationName("InTex");
  QCoreApplication::setOrganizationDomain("tu-dresden.de/et/intex");
  QCoreApplication::setApplicationName("InTex Ground Control");
  QApplication app(argc, argv);
  QGst::init(&argc, &argv);
  
  Control control;
  control.show();
  return app.exec();
}
