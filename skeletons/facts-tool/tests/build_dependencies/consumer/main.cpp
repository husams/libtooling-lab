#include <utility>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <nlohmann/json.hpp>
#include <iostream>

int main() {
  boost::asio::io_context context;
  boost::beast::http::response<boost::beast::http::string_body> response;
  boost::asio::post(context, [&] {
    response.result(boost::beast::http::status::ok);
    response.body() = nlohmann::json{{"status", "ok"}}.dump();
    response.prepare_payload();
  });
  context.run();
  if (response.result_int() != 200 || response.body().empty()) return 1;
  std::cout << response.body() << '\n';
}
