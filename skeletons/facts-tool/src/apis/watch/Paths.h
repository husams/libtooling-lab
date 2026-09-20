#pragma once
#include "apis/config/Settings.h"
#include <filesystem>

namespace facts::apis::watch {
std::filesystem::path absolute(const std::filesystem::path &path,
                               const Settings &settings);
bool ignored(const std::filesystem::path &path, const Settings &settings);
bool relevant(const std::filesystem::path &path);
}
