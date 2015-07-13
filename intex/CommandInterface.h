#pragma once

#include <QObject>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wundef"
#pragma clang diagnostic ignored "-Wnon-virtual-dtor"
#pragma clang diagnostic ignored "-Wweak-vtables"
#include "rpc/intex.capnp.h"
#pragma clang diagnostic pop

#include "VideoStreamSourceControl.h"
#include "IntexHardware.h"

class InTexServer final : public Command::Server {
  VideoStreamSourceControl source0;
  intex::hw::Valve valve0;
  intex::hw::Valve valve1;

public:
  InTexServer();
  kj::Promise<void> setPort(SetPortContext context) override;
  kj::Promise<void> setGPIO(SetGPIOContext context) override;
};
