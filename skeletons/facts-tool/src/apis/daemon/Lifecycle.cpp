#include "apis/daemon/Lifecycle.h"
#include "apis/daemon/Process.h"
#include <cerrno>
#include <cstring>
#include <unistd.h>

namespace facts::apis {
Lifecycle::~Lifecycle() {
  if (readiness_ >= 0) ::close(readiness_);
  if (lock_ < 0) return;
  // Keep the inode: unlinking an advisory lock permits parallel old/new owners.
  if (owner_) ::ftruncate(lock_, 0);
  ::close(lock_);
}
std::expected<bool, std::string> Lifecycle::start() {
  if (lock_ >= 0) return std::unexpected("server lifecycle was already started");
  return lockInstance(settings_.serverConfig.string() + ".pid")
      .and_then([&](int descriptor) -> std::expected<bool, std::string> {
        lock_ = descriptor;
        owner_ = true;
        if (settings_.daemon) return detach();
        return writePid(lock_).transform([] { return true; });
      });
}
std::expected<void, std::string> Lifecycle::ready(const std::string &host,
                                               std::uint16_t port) {
  if (readiness_ < 0) return {};
  const auto message = "OK facts-tool: server ready at " + host + ":" +
      std::to_string(port) + " (PID " + std::to_string(::getpid()) + ")\n";
  std::size_t written = 0;
  while (written < message.size()) {
    const auto count = ::write(readiness_, message.data() + written, message.size() - written);
    if (count < 0 && errno == EINTR) continue;
    if (count <= 0)
      return std::unexpected("cannot report daemon readiness: " + std::string(std::strerror(errno)));
    written += static_cast<std::size_t>(count);
  }
  ::close(readiness_);
  readiness_ = -1;
  return {};
}
}
