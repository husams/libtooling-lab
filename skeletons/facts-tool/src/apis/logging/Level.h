#pragma once
#include <expected>
#include <string>
#include <string_view>

namespace facts::apis::logging {
enum class Level { off, error, warning, info, debug, trace };
std::string_view levelName(Level level) noexcept;
std::expected<Level, std::string> parseLevel(std::string_view value);
Level verbosityLevel(unsigned verbosity) noexcept;
}
