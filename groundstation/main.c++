#include <string>

#include <QApplication>
#include <QString>

#include <boost/program_options.hpp>

#include "intex.h"
#include "qgst.h"
#include "Control.h"

int main(int argc, char *argv[]) {
  QCoreApplication::setOrganizationName("InTex");
  QCoreApplication::setOrganizationDomain("tu-dresden.de/et/intex");
  QCoreApplication::setApplicationName("InTex Ground Control");
  QApplication app(argc, argv);
  QGst::init(&argc, &argv);

  namespace po = boost::program_options;
  po::options_description desc("InTex Control options");
  // clang-format off
  desc.add_options()
    ("help", "print this help message")
    ("debug", "Enable debug mode")
    ("host", po::value<std::string>()->default_value(intex_host()),
     "InTex experiment host")
    ("port", po::value<uint16_t>()->default_value(intex_control_port()),
     "InTex experiment control port");
  // clang-format on

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);

  Control control(QString::fromStdString(vm["host"].as<std::string>()),
                  vm["port"].as<uint16_t>(), vm.count("debug") > 0);
  control.show();
  return app.exec();
}
