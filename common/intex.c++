#include "intex.h"

boost::asio::ip::address intex_ip() {
  return boost::asio::ip::address::from_string("172.16.18.162");
}

boost::asio::ip::address groundstation_ip() {
  return boost::asio::ip::address::from_string("172.16.18.163");
}

