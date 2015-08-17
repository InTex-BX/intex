#include <QDebug>
#include <QTimer>
#include <QString>

#include <chrono>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <chrono> // std::chrono::seconds
#include <thread> // std::this_thread::sleep_for

#include "IntexHardware.h"
int main() {
  // class intex::hw::gpio cs(intex::hw::config::ads1248_cs);
  // w1248_cs

  // class intex::hw::gpio BurnWire(intex::hw::config::burnwire);

  class ::intex::hw::BurnWire BurnWire(intex::hw::config::burnwire);
  class ::intex::hw::Valve Valve0(intex::hw::config::valve0);
  class ::intex::hw::Valve Valve1(intex::hw::config::valve1);

  std::cout << "Valve 0 on ... " << std::flush;
  Valve0.set(true);
  std::this_thread::sleep_for(std::chrono::seconds(3));
  Valve0.set(false);
  std::cout << "off" << std::endl;
  std::this_thread::sleep_for(std::chrono::seconds(3));

  std::cout << "Valve 1 on ... " << std::flush;
  Valve1.set(true);
  std::this_thread::sleep_for(std::chrono::seconds(3));
  Valve1.set(false);
  std::cout << "off" << std::endl;
  std::this_thread::sleep_for(std::chrono::seconds(3));

  std::cout << "Turn Burnwire on ... burning" << std::flush;
  BurnWire.actuate();
  std::cout << "done" << std::endl;

  return 0;
}
