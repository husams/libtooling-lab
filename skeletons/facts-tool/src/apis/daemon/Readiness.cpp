#include "apis/daemon/Process.h"
#include <cerrno>
#include <chrono>
#include <cstring>
#include <iostream>
#include <poll.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

namespace facts::apis {
std::expected<bool, std::string> awaitReadiness(int descriptor, pid_t child) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(60);
  std::string message;
  std::string failure = "daemon failed during startup";
  for (;;) {
    const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
        deadline - std::chrono::steady_clock::now()).count();
    if (remaining <= 0) { failure = "daemon startup timed out after 60 seconds"; break; }
    pollfd event{descriptor, POLLIN, 0};
    const int result = ::poll(&event, 1, static_cast<int>(remaining));
    if (result < 0 && errno == EINTR) continue;
    if (result == 0) { failure = "daemon startup timed out after 60 seconds"; break; }
    if (result < 0) { failure = std::strerror(errno); break; }
    char buffer[512];
    const auto count = ::read(descriptor, buffer, sizeof(buffer));
    if (count < 0 && errno == EINTR) continue;
    if (count <= 0) break;
    message.append(buffer, static_cast<std::size_t>(count));
    if (message.size() > 4096) { failure = "invalid daemon readiness response"; break; }
    if (message.ends_with('\n')) {
      if (!message.starts_with("OK ")) { failure = message; break; }
      std::cout << message.substr(3);
      return false;
    }
  }
  ::kill(child, SIGKILL);
  int status = 0;
  while (::waitpid(child, &status, 0) < 0 && errno == EINTR) {}
  return std::unexpected(failure);
}
}
