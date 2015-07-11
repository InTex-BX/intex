#pragma once

#include <kj/async-io.h>
#include <kj/async.h>

namespace intex {
namespace rpc {

struct QtAsyncIoContext {
  kj::Own<kj::LowLevelAsyncIoProvider> lowLevelProvider;
  kj::Own<kj::AsyncIoProvider> provider;
  kj::WaitScope &waitScope;
};

QtAsyncIoContext setupAsyncIo();
}
}
