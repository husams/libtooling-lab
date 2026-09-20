#pragma once
#include "apis/http/Router.h"
#include <boost/asio/ip/tcp.hpp>
#include <expected>

namespace facts::apis {
class Listener {
public:
  Listener(boost::asio::io_context &io, Router &router);
  std::expected<std::uint16_t, std::string> bind(const Settings &settings);
  void accept();
  void stop();
private:
  boost::asio::ip::tcp::acceptor acceptor_;
  Router &router_;
};
}
