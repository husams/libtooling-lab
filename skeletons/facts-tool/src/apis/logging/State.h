#pragma once
#include "apis/logging/Descriptor.h"
#include "apis/logging/Level.h"
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <thread>

namespace facts::apis::logging {
inline constexpr std::size_t queueLimit = 4096;
inline constexpr std::size_t recordLimit = 16384;
struct State {
  Level level = Level::off;
  Descriptor sink;
  std::mutex mutex;
  std::condition_variable ready;
  std::deque<std::string> records;
  std::uint64_t dropped = 0;
  bool stopping = false;
  std::thread worker;
};
void consume(State &state) noexcept;
}
