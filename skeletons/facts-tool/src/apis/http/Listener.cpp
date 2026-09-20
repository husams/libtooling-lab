#include "apis/http/Listener.h"
#include "apis/http/Session.h"
#include <fcntl.h>

namespace facts::apis {
Listener::Listener(boost::asio::io_context &io, Router &router)
    : acceptor_(io), router_(router) {}
std::expected<std::uint16_t, std::string> Listener::bind(const Settings &s) {
  using boost::asio::ip::tcp;
  boost::system::error_code ec;
  const auto address = boost::asio::ip::make_address(s.host, ec);
  if (ec) return std::unexpected("--host must be an IPv4 or IPv6 address");
  if (!address.is_loopback() && s.token.empty())
    return std::unexpected("A bearer token is required for non-loopback hosts");
  acceptor_.open(address.is_v6() ? tcp::v6() : tcp::v4(), ec);
  if (ec) return std::unexpected(ec.message());
  if (fcntl(acceptor_.native_handle(), F_SETFD, FD_CLOEXEC) == -1)
    return std::unexpected("Cannot protect listener descriptor");
  acceptor_.set_option(tcp::acceptor::reuse_address(true), ec);
  if (ec) return std::unexpected(ec.message());
  acceptor_.bind({address, s.port}, ec);
  if (ec == boost::asio::error::address_in_use && !s.explicitPort && s.port)
    acceptor_.bind({address, 0}, ec);
  if (ec) return std::unexpected(ec.message());
  acceptor_.listen(boost::asio::socket_base::max_listen_connections, ec);
  if (ec) return std::unexpected(ec.message());
  return acceptor_.local_endpoint().port();
}
void Listener::accept() {
  acceptor_.async_accept([this](auto ec, auto socket) {
    if (!ec) {
      fcntl(socket.native_handle(), F_SETFD, FD_CLOEXEC);
      std::make_shared<Session>(std::move(socket), router_)->start();
    }
    if (acceptor_.is_open()) accept();
  });
}
void Listener::stop() {
  boost::system::error_code ignored;
  acceptor_.close(ignored);
}
}
