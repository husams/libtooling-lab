#include "commands/DatabasePaths.h"

#include <filesystem>

namespace facts::commands {

std::expected<void, std::string>
validateDatabasePaths(std::string_view output, std::string_view configuration) {
  const auto outputPath = std::filesystem::absolute(output).lexically_normal();
  const auto configurationPath =
      std::filesystem::absolute(configuration).lexically_normal();
  if (outputPath == configurationPath) {
    return std::unexpected(
        "output and project configuration require separate databases");
  }
  std::error_code error;
  if (std::filesystem::equivalent(outputPath, configurationPath, error)) {
    return std::unexpected(
        "output and project configuration require separate databases");
  }
  return {};
}

} // namespace facts::commands
