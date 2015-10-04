#include <iostream>

#include <QCoreApplication>
#include <QObject>
#include <QTimer>
#include <QTime>

#include <boost/program_options.hpp>

#include "qgst.h"
#include "CommandInterface.h"
#include "rpc/ez-rpc.h"
#include "intex.h"

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

  std::cerr << QTime::currentTime() << " " << type
#ifndef QT_NO_DEBUG
            << " " << context.function << "(" << context.line << ")"
#endif
            << ": " << msg.toStdString() << std::endl;
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

  namespace po = boost::program_options;
  po::options_description desc("InTex Experiment options");
  // clang-format off
  desc.add_options()
    ("help", "print this help message")
    ("debug", "Enable debug mode")
    ("host", po::value<std::string>()->default_value(intex_host()),
     "InTex groundstation host");
  // clang-format on

  po::variables_map vm;
  po::store(po::command_line_parser(argc, argv)
                .options(desc)
                .allow_unregistered()
                .run(),
            vm);
  po::notify(vm);

  QTimer::singleShot(0, [&vm] {
    auto instance = kj::heap<InTexServer>(
        QString::fromStdString(vm["host"].as<std::string>()));
    server_instance = instance.get();
    intex::rpc::EzRpcServer server(kj::mv(instance), "*", 1234);
    auto &waitScope = server.getWaitScope();
    kj::NEVER_DONE.wait(waitScope);
  });

  application.exec();
}

