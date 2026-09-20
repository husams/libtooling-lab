#include "apis/config/Persistence.h"
#include "apis/config/Encode.h"
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace facts::apis {
namespace {
void writeAll(int descriptor, const std::string &text) {
  std::size_t offset = 0;
  while (offset < text.size()) {
    const auto count = ::write(descriptor, text.data() + offset, text.size() - offset);
    if (count < 0 && errno == EINTR) continue;
    if (count <= 0) throw std::runtime_error(std::strerror(errno));
    offset += static_cast<std::size_t>(count);
  }
}
}
std::expected<void, std::string> saveSettings(const Settings &settings) {
  int descriptor = -1;
  std::string temporary;
  try {
    const auto encoded = encodeSettings(settings);
    std::filesystem::create_directories(settings.serverConfig.parent_path());
    temporary = settings.serverConfig.string() + ".tmp.XXXXXX";
    descriptor = ::mkstemp(temporary.data());
    if (descriptor < 0) throw std::runtime_error(std::strerror(errno));
    if (::fchmod(descriptor, 0600) != 0 ||
        ::fcntl(descriptor, F_SETFD, FD_CLOEXEC) != 0)
      throw std::runtime_error(std::strerror(errno));
    writeAll(descriptor, encoded);
    if (::fsync(descriptor) != 0) throw std::runtime_error(std::strerror(errno));
    const int closed = ::close(descriptor);
    descriptor = -1;
    if (closed != 0) throw std::runtime_error(std::strerror(errno));
    std::filesystem::rename(temporary, settings.serverConfig);
    return {};
  } catch (const std::exception &error) {
    if (descriptor >= 0) ::close(descriptor);
    if (!temporary.empty()) ::unlink(temporary.c_str());
    return std::unexpected("cannot save server configuration " +
                           settings.serverConfig.string() + ": " + error.what());
  }
}
}
