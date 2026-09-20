#include "apis/http/Session.h"

namespace facts::apis {
Session::Session(boost::asio::ip::tcp::socket socket, Router &router)
    : stream_(std::move(socket)), router_(router) {
  parser_.body_limit(1024 * 1024);
  parser_.header_limit(16384);
}
void Session::start() {
  stream_.expires_after(std::chrono::seconds(30));
  boost::beast::http::async_read(stream_, buffer_, parser_,
      [self = shared_from_this()](auto ec, std::size_t) {
        if (ec) {
          if (ec != boost::beast::http::error::end_of_stream &&
              ec != boost::beast::error::timeout)
            self->respond(error(ec == boost::beast::http::error::body_limit
                                    ? 413 : 400, "Invalid HTTP request"));
          return;
        }
        try { self->respond(self->router_(self->parser_.get())); }
        catch (const std::exception &e) { self->respond(error(500, e.what())); }
      });
}
void Session::respond(Response response) {
  auto message = std::make_shared<Response>(std::move(response));
  stream_.expires_after(std::chrono::seconds(30));
  boost::beast::http::async_write(stream_, *message,
      [self = shared_from_this(), message](auto, std::size_t) {
        boost::system::error_code ignored;
        self->stream_.socket().shutdown(
            boost::asio::ip::tcp::socket::shutdown_both, ignored);
      });
}
}
