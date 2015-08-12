#include <iostream>

#include <QCoreApplication>
#include <QObject>
#include <QTimer>

#include "qgst.h"
#include "CommandInterface.h"
#include "rpc/ez-rpc.h"

static void output(QtMsgType type, const QMessageLogContext &,
                   const QString &msg) {
  if (server_instance)
    server_instance->syslog(type, msg);
  else
    std::cerr << msg.toStdString() << std::endl;
}

int main(int argc, char *argv[]) {
  QGst::init(&argc, &argv);
  QCoreApplication application(argc, argv);
  QCoreApplication::setOrganizationName("InTex");
  QCoreApplication::setOrganizationDomain("tu-dresden.de/et/intex");
  QCoreApplication::setApplicationName("InTex Experiment Control");
#ifdef BUILD_ON_RASPBERRY
  QCoreApplication::setApplicationVersion("(Raspberry)");
#else
  QCoreApplication::setApplicationVersion("(non-Raspberry)");
#endif

  qInstallMessageHandler(output);

  QTimer::singleShot(0, [] {
    intex::rpc::EzRpcServer server(kj::heap<InTexServer>(), "*", 1234);
    auto &waitScope = server.getWaitScope();
    kj::NEVER_DONE.wait(waitScope);
  });

  application.exec();
}

