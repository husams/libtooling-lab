#include "apis/http/Session.h"
#include "apis/generated/Limits.h"

namespace facts::apis {
Session::Session(boost::asio::ip::tcp::socket socket, Router &router)
    : stream_(std::move(socket)), router_(router) {
  parser_.body_limit(generated::maxBodyBytes);
  parser_.header_limit(generated::maxHeaderBytes);
}
void Session::start() {
  stream_.expires_after(std::chrono::seconds(30));
  boost::beast::http::async_read(stream_, buffer_, parser_,
      [self = shared_from_this()](auto ec, std::size_t bytes) {
        if (self->router_.logger) self->router_.logger->write(
            logging::Level::trace, "http.read",
            {{"bytes", bytes}, {"failed", bool(ec)}});
        if (ec) {
          if (ec != boost::beast::http::error::end_of_stream &&
              ec != boost::beast::error::timeout) {
            const auto status = ec == boost::beast::http::error::body_limit ? 413 : 400;
            if (self->router_.logger) self->router_.logger->write(
                logging::Level::warning, "http.invalid", {{"status", status}});
            self->respond(error(status, "Invalid HTTP request"));
          }
          return;
        }
        try {
          self->router_.handle(self->parser_.get(), [self](Response reply) {
            self->respond(std::move(reply));
          });
        }
        catch (const std::exception &) {
          if (self->router_.logger) self->router_.logger->write(
              logging::Level::error, "http.failed", {{"status", 500}});
          self->respond(error(500, "Internal server error"));
        }
      });
}
void Session::respond(Response response) {
  auto message = std::make_shared<Response>(std::move(response));
  stream_.expires_after(std::chrono::seconds(30));
  boost::beast::http::async_write(stream_, *message,
      [self = shared_from_this(), message](auto ec, std::size_t bytes) {
        if (self->router_.logger) self->router_.logger->write(
            logging::Level::trace, "http.write",
            {{"bytes", bytes}, {"failed", bool(ec)}, {"status", message->result_int()}});
        boost::system::error_code ignored;
        self->stream_.socket().shutdown(
            boost::asio::ip::tcp::socket::shutdown_both, ignored);
      });
}
}
