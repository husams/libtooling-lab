#pragma once
#include "apis/logging/Options.h"
#include <memory>
#include <nlohmann/json.hpp>
#include <string_view>

namespace facts::apis::logging {
struct State;
class Logger {
public:
  explicit Logger(const Options &options);
  ~Logger();
  Logger(const Logger &) = delete;
  Logger &operator=(const Logger &) = delete;
  void write(Level level, std::string_view event,
             nlohmann::json fields = nlohmann::json::object()) noexcept;
private:
  std::unique_ptr<State> state_;
};
}
