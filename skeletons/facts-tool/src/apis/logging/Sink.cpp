#include "apis/logging/Sink.h"
#include "apis/logging/Descriptor.h"
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <string_view>
#include <sys/stat.h>

namespace facts::apis::logging {
namespace {
std::unexpected<std::string> failure(std::string_view action) {
  return std::unexpected(std::string(action) + ": " + std::strerror(errno));
}
std::expected<int, std::string> directory(int parent, const std::string &name) {
  constexpr int flags = O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW;
  int descriptor = ::openat(parent, name.c_str(), flags);
  if (descriptor >= 0) return descriptor;
  if (errno != ENOENT) return failure("open logging directory");
  if (::mkdirat(parent, name.c_str(), 0700) != 0 && errno != EEXIST)
    return failure("create logging directory");
  descriptor = ::openat(parent, name.c_str(), flags);
  if (descriptor < 0) return failure("open logging directory");
  return descriptor;
}
std::expected<int, std::string> regularFile(int parent, const std::string &name) {
  constexpr int flags = O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC |
                        O_NOFOLLOW | O_NONBLOCK | O_NOCTTY;
  Descriptor descriptor(::openat(parent, name.c_str(), flags, 0600));
  if (descriptor.value < 0) return failure("open log file");
  struct stat attributes {};
  if (::fstat(descriptor.value, &attributes) != 0) return failure("inspect log file");
  if (!S_ISREG(attributes.st_mode))
    return std::unexpected("log destination must be a regular file");
  return descriptor.release();
}
}
std::expected<int, std::string> openSink(const std::filesystem::path &path) {
  if (path.empty()) {
    const int descriptor = ::fcntl(STDERR_FILENO, F_DUPFD_CLOEXEC, 3);
    if (descriptor < 0) return failure("duplicate logging stderr");
    return descriptor;
  }
  if (path.filename().empty() || path.filename() == "." || path.filename() == "..")
    return std::unexpected("log destination must name a regular file");
  Descriptor parent(::open(path.is_absolute() ? "/" : ".",
                           O_RDONLY | O_DIRECTORY | O_CLOEXEC));
  if (parent.value < 0) return failure("open logging root directory");
  for (const auto &component : path.parent_path().relative_path()) {
    if (component == ".") continue;
    auto next = directory(parent.value, component.string());
    if (!next) return std::unexpected(next.error());
    parent.reset(*next);
  }
  return regularFile(parent.value, path.filename().string());
}
}
