#pragma once
#include <expected>
#include <filesystem>
#include <string>
#include <sys/types.h>

namespace facts::apis {
std::expected<int, std::string> lockInstance(const std::filesystem::path &path);
std::expected<void, std::string> writePid(int descriptor);
std::expected<void, std::string> redirectDaemon(const std::filesystem::path &log,
                                             const std::filesystem::path &directory);
std::expected<bool, std::string> awaitReadiness(int descriptor, pid_t child);
}
