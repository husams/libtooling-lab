#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace facts::astcache::detail {
std::vector<std::string>
fingerprintArguments(const std::vector<std::string> &arguments,
                     const std::filesystem::path &source,
                     const std::filesystem::path &workingDirectory);
} // namespace facts::astcache::detail
