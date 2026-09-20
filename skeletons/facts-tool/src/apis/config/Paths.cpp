#include "apis/config/Paths.h"
#include <cstdlib>
#include <sstream>
#include <unistd.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

namespace facts::apis {
namespace {
namespace fs = std::filesystem;
fs::path executablePath(const char *argument) {
#ifdef __linux__
  std::error_code error;
  const auto executable = fs::read_symlink("/proc/self/exe", error);
  if (!error) return executable;
#elif defined(__APPLE__)
  std::uint32_t size = 0;
  _NSGetExecutablePath(nullptr, &size);
  std::string executable(size, '\0');
  if (_NSGetExecutablePath(executable.data(), &size) == 0)
    return fs::canonical(executable.c_str());
#endif
  if (fs::path(argument).has_parent_path()) return fs::canonical(argument);
  std::istringstream paths(std::getenv("PATH") ? std::getenv("PATH") : "");
  for (std::string path; std::getline(paths, path, ':');) {
    const auto candidate = fs::path(path) / argument;
    if (::access(candidate.c_str(), X_OK) == 0) return fs::canonical(candidate);
  }
  throw std::runtime_error("cannot resolve facts-tool executable");
}
fs::path absolutePath(const fs::path &path, const fs::path &base) {
  if (path.empty()) throw std::runtime_error("configuration paths must not be empty");
  return fs::weakly_canonical(path.is_absolute() ? path : base / path);
}
}
std::expected<Settings, std::string> normalizeSettings(Settings settings,
                                                    const char *executable) {
  try {
    const auto base = settings.serverConfig.parent_path();
    settings.workingDirectory = absolutePath(settings.workingDirectory, base);
    if (!fs::is_directory(settings.workingDirectory))
      throw std::runtime_error("working directory does not exist: " +
                               settings.workingDirectory.string());
    for (std::size_t i = 0; i < settings.defaults.size(); ++i) {
      if (settings.defaults[i] != "--config" && settings.defaults[i] != "--conf")
        continue;
      if (++i == settings.defaults.size())
        throw std::runtime_error("default configuration option is missing its path");
      settings.defaults[i] = absolutePath(settings.defaults[i],
                                         settings.workingDirectory).string();
    }
    if (settings.host.empty()) throw std::runtime_error("host must not be empty");
    if (settings.debounceMs < 1 || settings.debounceMs > 3600000)
      throw std::runtime_error("debounce_ms must be between 1 and 3600000");
    if (settings.timeoutSeconds < 1 || settings.timeoutSeconds > 86400)
      throw std::runtime_error("timeout_seconds must be between 1 and 86400");
    settings.executable = executablePath(executable);
    return settings;
  } catch (const std::exception &error) {
    return std::unexpected(error.what());
  }
}
}
