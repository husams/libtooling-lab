#include "apis/logging/Record.h"
#include "apis/logging/State.h"
#include <chrono>

namespace facts::apis::logging {
std::string record(Level level, std::string_view event, nlohmann::json fields) {
  using namespace std::chrono;
  nlohmann::json body{
      {"timestamp", duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count()},
      {"level", levelName(level)}, {"event", event.substr(0, 256)},
      {"fields", fields.is_object() ? std::move(fields) : nlohmann::json::object()}};
  auto result = body.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
  if (result.size() >= recordLimit) {
    body["fields"] = {{"truncated", true}};
    result = body.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
  }
  return result + '\n';
}
}
