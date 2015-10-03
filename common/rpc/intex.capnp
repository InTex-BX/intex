#interface intex {
#
#}

@0xdd875d35b456aabd;

using Cxx = import "/capnp/c++.capnp";

struct Exception {
  reason @0 :Text;
}

struct Pressure {
  value @0: Float64;
}

struct Temperature {
  value @0: Float64;
}

struct Status {
  value @0: Bool;
}

struct Reading(Type) {
  timestamp @0 :Int64;
  union {
    reading @1 :Type;
    error @2 :Exception;
  }
}

struct Telemetry {
  cpuTemperature @0 :Reading(Temperature);
  vnaTemperature @1 :Reading(Temperature);
  boxTemperature @2 :Reading(Temperature);
  antennaInnerTemperature @3 :Reading(Temperature);
  antennaOuterTemperature @4 :Reading(Temperature);
  tankPressure @5 :Reading(Temperature);
  antennaPressure @6 :Reading(Temperature);
  atmosphericPressure @7 :Reading(Temperature);
  innerHeater @8 :Reading(Status);
  outerHeater @9 :Reading(Status);
  tankValve @10 :Reading(Status);
  outletValve @11 :Reading(Status);
  burnwire @12 :Reading(Status);
}

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
  telemetry @2;
  log @3;
}

enum InTexHW {
  valve0 @0;
  valve1 @1;
  heater0 @2;
  heater1 @3;
  burnwire @4;
  minivna @5;
  usbhub @6;
}

enum InTexFeed {
  feed0 @0;
  feed1 @1;
}

interface Command {
  setPort @0 (service: InTexService, port: UInt16);
  setGPIO @1 (port: InTexHW, on: Bool);
  setBitrate @2 (feed: InTexFeed, bitrate: UInt32);
  setVolume @3(feed: InTexFeed, volume: Float32);
  start @4 (service: InTexService);
  stop @5 (service: InTexService);
  next @6 (service: InTexService);
}
