#include "apis/logging/Level.h"
#include <array>

namespace facts::apis::logging {
namespace {
constexpr std::array<std::string_view, 6> names{
    "off", "error", "warning", "info", "debug", "trace"};
}
std::string_view levelName(Level level) noexcept {
  const auto index = static_cast<unsigned>(level);
  return index < names.size() ? names[index] : "off";
}
std::expected<Level, std::string> parseLevel(std::string_view value) {
  for (unsigned index = 0; index < names.size(); ++index)
    if (names[index] == value) return static_cast<Level>(index);
  return std::unexpected("logging.level must be off, error, warning, info, debug, or trace");
}
Level verbosityLevel(unsigned verbosity) noexcept {
  constexpr std::array levels{Level::error, Level::info, Level::debug, Level::trace};
  return levels[verbosity < levels.size() ? verbosity : levels.size() - 1];
}
}
