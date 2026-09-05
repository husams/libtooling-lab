#pragma once
#include <expected>
#include <filesystem>
#include <string>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#include <utility>

namespace facts::config::detail {
// Lock the existing parent directory, without leaving an ownership sidecar.
class ParentLock {
public:
  explicit ParentLock(int fd) : fd_(fd) {}
  ParentLock(ParentLock &&other) : fd_(std::exchange(other.fd_, -1)) {}
  ~ParentLock() { if (fd_ >= 0) { flock(fd_, LOCK_UN); close(fd_); } }
  static std::expected<ParentLock, std::string> acquire(const std::filesystem::path &parent) {
    const int fd = open(parent.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0) return std::unexpected("cannot open generated conf parent");
    if (flock(fd, LOCK_EX) == 0) return ParentLock(fd);
    close(fd);
    return std::unexpected("cannot lock generated conf parent");
  }
private:
  int fd_;
};
} // namespace facts::config::detail
