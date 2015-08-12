#interface intex {
#
#}

@0xdd875d35b456aabd;

using Cxx = import "/capnp/c++.capnp";

struct Message {
  union {
    startstream :group {
      port0 @0 :UInt16;
      port1 @1 :UInt16;
      bitrate @2 :UInt32;
    }
    changestream :group {
      bitrate @3 :UInt32;
    }
  }
}

enum InTexService {
  videoFeed0 @0;
  videoFeed1 @1;
  tankPressure @2;
  atmosphericPressure @3;
  innerTemperature @4;
  outerTemperature @5;
  log @6;
}

enum InTexHW {
  valve0 @0;
  valve1 @1;
  heater0 @2;
  heater1 @3;
  burnwire @4;
}

enum InTexFeed {
  feed0 @0;
  feed1 @1;
}

interface Command {
  setPort @0 (service: InTexService, port: UInt16);
  setGPIO @1 (port: InTexHW, on: Bool);
  setBitrate @2 (feed: InTexFeed, bitrate: UInt32);
  start @3 (service: InTexService);
  stop @4 (service: InTexService);
  next @5 (service: InTexService);
}
