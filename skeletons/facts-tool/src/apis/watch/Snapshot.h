#pragma once
#include "apis/watch/Scan.h"

namespace facts::apis::watch {
std::expected<std::string, std::string> fingerprint(const std::filesystem::path &);
std::string signature(const Settings &, const Scan &);
bool restored(const Settings &, const Scan &);
std::expected<void, std::string> forget(const Settings &);
std::expected<void, std::string> remember(const Settings &, const Scan &);
}
