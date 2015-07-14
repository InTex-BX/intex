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

class RpcServer : public QObject, public intex::rpc::EzRpcServer {
  Q_OBJECT

public:
  RpcServer(kj::StringPtr host, ::capnp::uint port)
      : EzRpcServer(kj::heap<InTexServer>(), host, port) {}
  ~RpcServer() noexcept {}
  [[noreturn]] void run() {
    auto &waitScope = getWaitScope();
    kj::NEVER_DONE.wait(waitScope);
  }
};

int main(int argc, char *argv[]) {
  QGst::init(&argc, &argv);
  QCoreApplication application(argc, argv);

  qInstallMessageHandler(output);

  RpcServer server("*", 1234);

  QTimer::singleShot(0, &server, &RpcServer::run);

  application.exec();
}

#include "main.moc"
