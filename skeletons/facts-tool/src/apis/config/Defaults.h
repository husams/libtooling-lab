#pragma once
#include <expected>
#include <string>
#include <vector>

namespace facts::apis {
std::expected<void, std::string>
validateDefaults(const std::vector<std::string> &defaults);
}
