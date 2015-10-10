#include <iostream>
#include <string>

#include <cerrno>
#include <cstring>
#include <cstdlib>

#include <fcntl.h>

#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>

#include <capnp/serialize.h>
//#include <capnp/common.h>
//#include <capnp/message.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wweak-vtables"
#pragma clang diagnostic ignored "-Wnon-virtual-dtor"
#include "rpc/intex.capnp.h"
#pragma clang diagnostic pop

static int64_t base_time;
static int64_t divisor = 1;

enum class temperature {
  CPU,
  VNA,
  INNER_HEATER,
  OUTER_HEATER,
  ATMOSPHERE,
  HUB_PCB
};

enum class pressure { TANK, ATMOSPHERE, ANTENNA };

static std::ostream &operator<<(std::ostream &os, const enum temperature &t) {
  switch (t) {
  case temperature::CPU:
    return os << "cpu_temperature[C]";
  case temperature::VNA:
    return os << "vna_temperature[C]";
  case temperature::INNER_HEATER:
    return os << "inner_heater[C]";
  case temperature::OUTER_HEATER:
    return os << "outer_heater[C]";
  case temperature::ATMOSPHERE:
    return os << "atmosphere[C]";
  case temperature::HUB_PCB:
    return os << "hub[C]";
  }
}

static std::ostream &operator<<(std::ostream &os, const enum pressure &p) {
  switch (p) {
  case pressure::TANK:
    return os << "tank[Bar]";
  case pressure::ATMOSPHERE:
    return os << "atmosphere[Bar]";
  case pressure::ANTENNA:
    return os << "antenna[Bar]";
  }
}

static std::istream &operator>>(std::istream &in, enum temperature &type) {
  std::string token;
  in >> token;
  if (boost::iequals(token, "cpu"))
    type = temperature::CPU;
  else if (boost::iequals(token, "vna"))
    type = temperature::VNA;
  else if (boost::iequals(token, "inner-heater-ring"))
    type = temperature::INNER_HEATER;
  else if (boost::iequals(token, "outer-heater-ring"))
    type = temperature::OUTER_HEATER;
  else if (boost::iequals(token, "atmosphere"))
    type = temperature::ATMOSPHERE;
  else if (boost::iequals(token, "hub"))
    type = temperature::HUB_PCB;
  else
    throw boost::program_options::validation_error(
        boost::program_options::validation_error::invalid_option_value);
  return in;
}

static std::istream &operator>>(std::istream &in, enum pressure &type) {
  std::string token;
  in >> token;
  if (boost::iequals(token, "tank"))
    type = pressure::TANK;
  else if (boost::iequals(token, "atmosphere"))
    type = pressure::ATMOSPHERE;
  else if (boost::iequals(token, "antenna"))
    type = pressure::ANTENNA;
  else
    throw boost::program_options::validation_error(
        boost::program_options::validation_error::invalid_option_value,
        "Invalid pressure", token);
  return in;
}

static std::ostream &
operator<<(std::ostream &os,
           const typename Reading<Temperature>::Reader &measurement) {
  os << measurement.getTimestamp() / divisor - base_time << " ";
  if (measurement.hasError())
    return os << "nan" << std::endl;
  else
    return os << measurement.getReading().getValue() << std::endl;
}

int main(int argc, char *argv[]) {
  namespace po = boost::program_options;
  po::options_description desc("InTex Telemetry data converter options");
  // clang-format off
  desc.add_options()
    ("help", "Print this help message")
    ("base-time,b", po::value<int64_t>(&base_time)->default_value(0),
     "Subtract this time from all time stamps,"
     "making it T-0 (default: 0). The unit is nanoseconds, except if the "
     "--milliseconds or --microseconds options is specified, then it's ms or "
     "us, respectively")
    ("temperature,t", po::value<temperature>(),
     "Temperature data to export. Valid options are:"
     "cpu, vna, inner-heater-ring, outer-heater-ring, atmosphere, and hub")
    ("pressure,p", po::value<pressure>(),
     "Pressure data to export. Valid options are:"
     "tank, atmopshere, antenna")
    ("milliseconds,m", "Output timestamps in milliseconds.")
    ("microseconds,u", "Output timestamps in microseconds.")
    ("input-file,i", po::value<std::string>(),
     "Telemetry file to extract data from");
  // clang-format on

  po::positional_options_description positionals;
  positionals.add("input-file", 1);

  po::variables_map vm;
  po::store(po::command_line_parser(argc, argv)
                .options(desc)
                .positional(positionals)
                .run(),
            vm);
  po::notify(vm);

  if (vm.count("help")) {
    std::cout << desc << std::endl;
    return EXIT_SUCCESS;
  }

  if (vm.count("pressure") && vm.count("temperature")) {
    std::cout << "Pressure and temperature specified at the same time. This "
                 "program can only export one data type at a time."
              << std::endl;
    return EXIT_FAILURE;
  }

  if (vm.count("milliseconds") && vm.count("microseconds")) {
    std::cout << "You can only specify millisecond or microsecond timestamps."
              << std::endl;
    return EXIT_FAILURE;
  }

  const std::string fname = vm["input-file"].as<std::string>();
  const int fd = open(fname.c_str(), O_RDONLY);
  if(fd < 0) {
    std::cout << "Could not open file '" << fname << "': " << strerror(errno)
              << " (" << errno << ")." << std::endl;
    return EXIT_FAILURE;
  }

  std::cout << "# time[";
  if (vm.count("microseconds")) {
    divisor *= 1'000;
    std::cout << "us";
  } else if (vm.count("milliseconds")) {
    divisor *= 1'000'000;
    std::cout << "ms";
  } else {
    std::cout << "ns";
  }
  std::cout << "] ";

  if (vm.count("pressure"))
    std::cout << vm["pressure"].as<pressure>();
  if (vm.count("temperature"))
    std::cout << vm["temperature"].as<temperature>();
  std::cout << std::endl;

  for (;;) {
    capnp::StreamFdMessageReader reader(fd);
    auto telemetry = reader.getRoot<Telemetry>();

    if (vm.count("pressure")) {
      switch (vm["pressure"].as<pressure>()) {
      case pressure::TANK:
        std::cout << telemetry.getTankPressure();
        break;
      case pressure::ATMOSPHERE:
        std::cout << telemetry.getAtmosphericPressure();
        break;
      case pressure::ANTENNA:
        std::cout << telemetry.getAntennaPressure();
        break;
      }
    }

    if (vm.count("temperature")) {
      switch (vm["temperature"].as<temperature>()) {
      case temperature::CPU:
        std::cout << telemetry.getCpuTemperature();
        break;
      case temperature::VNA:
        std::cout << telemetry.getVnaTemperature();
        break;
      case temperature::INNER_HEATER:
        std::cout << telemetry.getAntennaInnerTemperature();
        break;
      case temperature::OUTER_HEATER:
        std::cout << telemetry.getAntennaOuterTemperature();
        break;
      case temperature::ATMOSPHERE:
        std::cout << telemetry.getAtmosphereTemperature();
        break;
      case temperature::HUB_PCB:
        std::cout << telemetry.getBoxTemperature();
        break;
      }
    }
  }
}
