#include "apis/config/Logging.h"
#include <stdexcept>

namespace facts::apis {
namespace {
namespace fs = std::filesystem;
bool sameFile(const fs::path &left, const fs::path &right) {
  std::error_code error;
  return left == right || fs::equivalent(left, right, error);
}
void rejectCollision(const fs::path &file, const fs::path &protectedFile) {
  if (sameFile(file, protectedFile))
    throw std::runtime_error("logging.file conflicts with server data: " +
                             protectedFile.string());
}
}
void normalizeLogging(Settings &settings) {
  auto &file = settings.logging.file;
  if (file.empty() && settings.daemon) file = settings.serverConfig.string() + ".log";
  if (file.empty()) return;
  if (file.string().find('\0') != std::string::npos)
    throw std::runtime_error("logging.file must contain no NUL");
  if (file.is_relative()) file = settings.serverConfig.parent_path() / file;
  file = file.lexically_normal();
  if (file.filename().empty())
    throw std::runtime_error("logging.file must name a regular file");
  file = fs::weakly_canonical(file.parent_path()) / file.filename();
  const auto status = fs::symlink_status(file);
  if (fs::exists(status) && !fs::is_regular_file(status))
    throw std::runtime_error("logging.file must be a regular file, not a link or directory");
  rejectCollision(file, settings.serverConfig);
  rejectCollision(file, settings.serverConfig.string() + ".pid");
  rejectCollision(file, settings.executable);
  for (std::size_t index = 1; index < settings.defaults.size(); index += 2)
    rejectCollision(file, settings.defaults[index]);
}
}
