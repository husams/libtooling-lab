#pragma once

#include <expected>
#include <string>
#include <string_view>

namespace facts::commands {

std::expected<void, std::string>
validateDatabasePaths(std::string_view output, std::string_view configuration);

} // namespace facts::commands
