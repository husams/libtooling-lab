#pragma once
#include <expected>
#include <string>
#include <string_view>

namespace facts::commands {
std::expected<void, std::string> writeGraphOutput(const std::string &path,
                                                  std::string_view text);
} // namespace facts::commands
