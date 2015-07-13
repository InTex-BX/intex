#include <iostream>
#include <stdexcept>
#include <thread>
#include <chrono>

#include "CommandInterface.h"
#include "VideoStreamSourceControl.h"

InTexServer::InTexServer()
    : source0("127.0.0.1", "5000", ""), valve0(intex::hw::config::valve0),
      valve1(intex::hw::config::valve1) {}

kj::Promise<void> InTexServer::setPort(SetPortContext context) {
  std::cout << __PRETTY_FUNCTION__ << " "
            << static_cast<int>(context.getParams().getService()) << " "
            << context.getParams().getPort() << std::endl;
  auto params = context.getParams();
  switch (params.getService()) {
  case InTexService::VIDEO_FEED0:
    source0.setPort(params.getPort());
    return kj::READY_NOW;
    break;
  }
  throw std::runtime_error("Port not implemented.");
}

kj::Promise<void> InTexServer::setGPIO(SetGPIOContext context) {
  using namespace std::chrono;
  std::cout << __PRETTY_FUNCTION__ << std::endl;
  std::this_thread::sleep_for(0.1s);
  auto params = context.getParams();
  switch (params.getPort()) {
  case InTexHW::VALVE0:
    valve0.set(params.getOn());
    return kj::READY_NOW;
  case InTexHW::VALVE1:
    valve1.set(params.getOn());
    return kj::READY_NOW;
  }
  throw std::runtime_error("GPIO not implemented.");
}
