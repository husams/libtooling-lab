#pragma once

#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace facts::storage::driverprobe {

using IncludePaths = std::vector<std::filesystem::path>;

std::expected<std::optional<IncludePaths>, std::string>
read(const std::filesystem::path &project, std::string_view key);

std::expected<void, std::string>
write(const std::filesystem::path &project, std::string_view key,
       const IncludePaths &includes);

} // namespace facts::storage::driverprobe
