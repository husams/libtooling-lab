#include "apis/daemon/Process.h"
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

namespace facts::apis {
std::expected<int, std::string> lockInstance(const std::filesystem::path &path) {
  std::error_code error;
  std::filesystem::create_directories(path.parent_path(), error);
  if (error) return std::unexpected("cannot create server directory: " + error.message());
  const int descriptor = ::open(path.c_str(), O_CREAT | O_RDWR | O_CLOEXEC | O_NOFOLLOW, 0600);
  if (descriptor < 0)
    return std::unexpected("cannot open PID file " + path.string() + ": " + std::strerror(errno));
  if (::flock(descriptor, LOCK_EX | LOCK_NB) != 0) {
    const auto reason = std::string(std::strerror(errno));
    ::close(descriptor);
    return std::unexpected("server instance already active or PID lock unavailable: " + reason);
  }
  if (::fchmod(descriptor, 0600) != 0) {
    ::close(descriptor);
    return std::unexpected("cannot restrict PID file permissions");
  }
  return descriptor;
}
std::expected<void, std::string> writePid(int descriptor) {
  const auto text = std::to_string(::getpid()) + "\n";
  if (::ftruncate(descriptor, 0) != 0 ||
      ::pwrite(descriptor, text.data(), text.size(), 0) != static_cast<ssize_t>(text.size()))
    return std::unexpected("cannot write server PID: " + std::string(std::strerror(errno)));
  return {};
}
}
