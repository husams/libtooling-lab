#pragma once
#include "apis/config/Settings.h"
#include <expected>

namespace facts::apis {
std::expected<Settings, std::string>
loadSettings(const std::filesystem::path &path);
std::expected<void, std::string> saveSettings(const Settings &settings);
}
