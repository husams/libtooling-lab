#include "apis/logging/State.h"
#include "apis/logging/Record.h"
#include <cerrno>
#include <csignal>
#include <pthread.h>
#include <utility>

namespace facts::apis::logging {
namespace {
bool append(int descriptor, std::string_view value) noexcept {
  while (!value.empty()) {
    const auto count = ::write(descriptor, value.data(), value.size());
    if (count < 0 && errno == EINTR) continue;
    if (count <= 0) return false;
    value.remove_prefix(static_cast<std::size_t>(count));
  }
  return true;
}
void emit(State &state, std::string_view value) noexcept {
  if (state.sink.value < 0 || append(state.sink.value, value)) return;
  state.sink.reset(-1);
  constexpr std::string_view message = "facts-tool: server logging sink failed; further logs discarded\n";
  static_cast<void>(append(STDERR_FILENO, message));
}
void blockPipeSignal() noexcept {
  sigset_t signals;
  ::sigemptyset(&signals);
  ::sigaddset(&signals, SIGPIPE);
  static_cast<void>(::pthread_sigmask(SIG_BLOCK, &signals, nullptr));
}
}
void consume(State &state) noexcept {
  blockPipeSignal();
  try {
    for (;;) {
      std::string value;
      std::uint64_t dropped = 0;
      {
        std::unique_lock lock(state.mutex);
        state.ready.wait(lock, [&] {
          return state.stopping || !state.records.empty() || state.dropped;
        });
        dropped = std::exchange(state.dropped, 0);
        if (!state.records.empty()) {
          value = std::move(state.records.front());
          state.records.pop_front();
        } else if (state.stopping && dropped == 0) break;
      }
      if (dropped) emit(state, record(state.level == Level::error ? Level::error : Level::warning,
                                      "logger.dropped", {{"records", dropped}}));
      if (!value.empty()) emit(state, value);
    }
  } catch (...) {
    emit(state, "{\"level\":\"error\",\"event\":\"logger.failed\"}\n");
  }
}
}
