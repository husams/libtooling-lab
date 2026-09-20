#include "apis/daemon/Process.h"
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace facts::apis {
std::expected<void, std::string> redirectDaemon(const std::filesystem::path &log,
                                             const std::filesystem::path &directory) {
  ::umask(0077);
  if (::setsid() < 0 || ::chdir(directory.c_str()) != 0)
    return std::unexpected("cannot detach server: " + std::string(std::strerror(errno)));
  const int input = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
  if (input < 0) return std::unexpected("cannot open daemon input");
  const int output = ::open(log.c_str(), O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC | O_NOFOLLOW, 0600);
  if (output < 0) {
    ::close(input);
    return std::unexpected("cannot open daemon log: " + std::string(std::strerror(errno)));
  }
  const bool success = ::fchmod(output, 0600) == 0 &&
      ::dup2(input, STDIN_FILENO) >= 0 && ::dup2(output, STDOUT_FILENO) >= 0 &&
      ::dup2(output, STDERR_FILENO) >= 0;
  if (input > STDERR_FILENO) ::close(input);
  if (output > STDERR_FILENO) ::close(output);
  if (!success) return std::unexpected("cannot redirect daemon standard streams");
  return {};
}
}
