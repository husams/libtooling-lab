#include "apis/logging/Logger.h"
#include "apis/logging/Record.h"
#include "apis/logging/Sink.h"
#include "apis/logging/State.h"
#include <stdexcept>

namespace facts::apis::logging {
Logger::Logger(const Options &options) : state_(std::make_unique<State>()) {
  state_->level = options.level;
  if (options.level == Level::off && options.file.empty()) return;
  auto sink = openSink(options.file);
  if (!sink) throw std::runtime_error(sink.error());
  state_->sink.reset(*sink);
  if (options.level == Level::off) return;
  state_->worker = std::thread([this] { consume(*state_); });
}
Logger::~Logger() {
  {
    const std::lock_guard lock(state_->mutex);
    state_->stopping = true;
  }
  state_->ready.notify_one();
  if (state_->worker.joinable()) state_->worker.join();
}
void Logger::write(Level level, std::string_view event, nlohmann::json fields) noexcept {
  if (level == Level::off || level > state_->level) return;
  try {
    auto value = record(level, event, std::move(fields));
    {
      const std::lock_guard lock(state_->mutex);
      if (state_->stopping) return;
      if (state_->records.size() >= queueLimit) ++state_->dropped;
      else state_->records.push_back(std::move(value));
    }
    state_->ready.notify_one();
  } catch (...) {
    // Reporting serialization/allocation failure must never escape an API callback.
    try {
      const std::lock_guard lock(state_->mutex);
      ++state_->dropped;
      state_->ready.notify_one();
    } catch (...) {}
  }
}
}
