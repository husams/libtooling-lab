#pragma once

#include <expected>
#include <filesystem>
#include <string>
#include <vector>

namespace facts::platform {

std::expected<std::string, std::string>
driverProbeKey(const std::filesystem::path &driver,
               const std::filesystem::path &directory,
               const std::vector<std::string> &options);

std::vector<std::string>
resolveProbeOptions(const std::vector<std::string> &options,
                    const std::filesystem::path &directory);

} // namespace facts::platform
