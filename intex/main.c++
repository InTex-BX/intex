#include <iostream>

#include <QCoreApplication>
#include <QObject>
#include <QTimer>
#include <QTime>

#include "qgst.h"
#include "CommandInterface.h"
#include "rpc/ez-rpc.h"

static std::ostream &operator<<(std::ostream &os, const QTime &time) {
  return os << time.toString(Qt::ISODate).toStdString();
}

static std::ostream &operator<<(std::ostream &os, const QtMsgType type) {
  switch (type) {
  case QtDebugMsg:
    return os << "[DD]";
#if QT_VERSION >= 0x050500
  case QtInfoMsg:
    return os << "[II]";
#endif
  case QtWarningMsg:
    return os << "[WW]";
  case QtCriticalMsg:
    return os << "[CC]";
  case QtFatalMsg:
    return os << "[EE]";
  }
}

static void output(QtMsgType type, const QMessageLogContext &context,
                   const QString &msg) {
  if (server_instance)
    server_instance->syslog(type, msg);

  std::cerr << QTime::currentTime() << " " << type << " " << context.function
            << "(" << context.line << "): " << msg.toStdString() << std::endl;
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

