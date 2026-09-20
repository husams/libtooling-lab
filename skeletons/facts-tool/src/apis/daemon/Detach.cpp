#include "apis/daemon/Lifecycle.h"
#include "apis/daemon/Process.h"
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <signal.h>
#include <unistd.h>

namespace facts::apis {
std::expected<bool, std::string> Lifecycle::detach() {
  int descriptors[2];
  if (::pipe(descriptors) != 0)
    return std::unexpected("cannot create readiness pipe: " + std::string(std::strerror(errno)));
  if (::fcntl(descriptors[0], F_SETFD, FD_CLOEXEC) != 0 ||
      ::fcntl(descriptors[1], F_SETFD, FD_CLOEXEC) != 0) {
    ::close(descriptors[0]); ::close(descriptors[1]);
    return std::unexpected("cannot protect readiness descriptors");
  }
  std::cout.flush();
  std::cerr.flush();
  const auto child = ::fork();
  if (child < 0) {
    ::close(descriptors[0]); ::close(descriptors[1]);
    return std::unexpected("cannot fork daemon: " + std::string(std::strerror(errno)));
  }
  if (child > 0) {
    owner_ = false;
    ::close(descriptors[1]);
    const auto result = awaitReadiness(descriptors[0], child);
    ::close(descriptors[0]);
    if (!result)
      return std::unexpected(result.error() + "; inspect " + settings_.logging.file.string());
    return *result;
  }
  ::close(descriptors[0]);
  readiness_ = descriptors[1];
  ::signal(SIGPIPE, SIG_IGN);
  return redirectDaemon(settings_.logging.file.string(), settings_.workingDirectory)
      .and_then([&] { return writePid(lock_); })
      .transform([] { return true; });
}
}
