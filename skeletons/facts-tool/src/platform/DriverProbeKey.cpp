#include "platform/DriverProbeKey.h"

#include <llvm/ADT/StringRef.h>
#include <llvm/Support/SHA256.h>

#include <array>
#include <cstdlib>
#include <sstream>
#include <string_view>

namespace facts::platform {
namespace {

void field(std::ostringstream &stream, std::string_view value) {
  stream << value.size() << ':' << value;
}

std::string absoluteOption(std::string value,
                           const std::filesystem::path &directory) {
  if (value.empty() || std::filesystem::path(value).is_absolute())
    return value;
  return (directory / value).string();
}

std::string digest(const std::string &bytes) {
  llvm::SHA256 hash;
  hash.update(bytes);
  constexpr char digits[] = "0123456789abcdef";
  std::string result;
  for (const auto byte : hash.final()) {
    result += digits[byte >> 4U];
    result += digits[byte & 15U];
  }
  return result;
}

} // namespace

std::vector<std::string>
resolveProbeOptions(const std::vector<std::string> &options,
                    const std::filesystem::path &directory) {
  auto resolved = options;
  for (std::size_t index = 0; index < resolved.size(); ++index) {
    const std::string_view option(resolved[index]);
    if ((option == "--sysroot" || option == "-isysroot" || option == "-B") &&
        index + 1 < resolved.size()) {
      ++index;
      resolved[index] = absoluteOption(resolved[index], directory);
    } else if (option.starts_with("--sysroot=")) {
      resolved[index] = "--sysroot=" +
                        absoluteOption(resolved[index].substr(10), directory);
    } else if (option.starts_with("-isysroot") && option.size() > 9) {
      resolved[index] = "-isysroot" +
                        absoluteOption(resolved[index].substr(9), directory);
    } else if (option.starts_with("-B") && option.size() > 2) {
      resolved[index] = "-B" + absoluteOption(resolved[index].substr(2), directory);
    }
  }
  return resolved;
}

std::expected<std::string, std::string>
driverProbeKey(const std::filesystem::path &driver,
               const std::filesystem::path &directory,
               const std::vector<std::string> &options) {
  std::error_code error;
  const auto canonical = std::filesystem::canonical(driver, error);
  if (error)
    return std::unexpected("cannot identify GNU driver: " + error.message());
  const auto size = std::filesystem::file_size(canonical, error);
  if (error)
    return std::unexpected("cannot stat GNU driver: " + error.message());
  const auto modified = std::filesystem::last_write_time(canonical, error);
  if (error)
    return std::unexpected("cannot stat GNU driver: " + error.message());
  std::ostringstream key;
  field(key, "gnu-cxx-includes-v1");
  field(key, driver.string());
  field(key, canonical.string());
  field(key, std::to_string(size));
  field(key, std::to_string(modified.time_since_epoch().count()));
  field(key, directory.string());
  for (const auto &option : options)
    field(key, option);
  constexpr std::array environment{
      "PATH", "GCC_EXEC_PREFIX", "COMPILER_PATH", "LIBRARY_PATH", "CPATH",
      "CPLUS_INCLUDE_PATH", "C_INCLUDE_PATH", "OBJC_INCLUDE_PATH",
      "SDKROOT", "MACOSX_DEPLOYMENT_TARGET", "LANG", "LC_ALL", "LC_MESSAGES"};
  for (const auto name : environment) {
    field(key, name);
    const auto value = std::getenv(name);
    field(key, value ? "set" : "unset");
    field(key, value ? value : "");
  }
  return digest(key.str());
}

} // namespace facts::platform
