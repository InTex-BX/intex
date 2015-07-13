#include <iostream>

#include <QCoreApplication>
#include <QTimer>

#include "qgst.h"
#include "CommandInterface.h"
#include "rpc/ez-rpc.h"

int main(int argc, char *argv[]) {
  QGst::init(&argc, &argv);
  QCoreApplication application(argc, argv);

  QTimer::singleShot(0, [] {
    // Set up the EzRpcServer, binding to port 5923 unless a
    // different port was specified by the user.
    intex::rpc::EzRpcServer server(kj::heap<InTexServer>(), "*", 1234);
    auto &waitScope = server.getWaitScope();

    // Export a capability under the name "foo".  Note that the
    // second parameter here can be any "Client" object or anything
    // that can implicitly cast to a "Client" object.  You can even
    // re-export a capability imported from another server.
    // server.exportCap("foo", kj::heap<MyInterfaceImpl>());

    // Run forever, accepting connections and handling requests.
    kj::NEVER_DONE.wait(waitScope);
  });

  application.exec();
}
