#pragma once
#include "apis/http/Router.h"
#include <boost/beast/core.hpp>
#include <memory>

namespace facts::apis {
class Session : public std::enable_shared_from_this<Session> {
public:
  Session(boost::asio::ip::tcp::socket socket, Router &router);
  void start();
private:
  void respond(Response response);
  boost::beast::tcp_stream stream_;
  boost::beast::flat_buffer buffer_;
  boost::beast::http::request_parser<boost::beast::http::string_body> parser_;
  Router &router_;
};
}
