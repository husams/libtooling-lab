#pragma once
#include <expected>
#include <filesystem>
#include <string>
#include <sys/types.h>
#include <vector>

namespace facts::apis {
struct Child {
  pid_t pid;
  int output;
  int error;
};
std::expected<Child, std::string> spawnChild(
    const std::filesystem::path &executable,
    const std::vector<std::string> &arguments,
    const std::filesystem::path &workingDirectory = {});
}
