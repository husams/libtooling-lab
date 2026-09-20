#pragma once
#include <expected>
#include <filesystem>
#include <string>

namespace facts::apis::logging {
// The caller owns the returned descriptor. Empty paths duplicate stderr.
std::expected<int, std::string> openSink(const std::filesystem::path &path);
}
