#pragma once
#include "apis/config/Settings.h"
#include <expected>

namespace facts::apis {
std::expected<Settings, std::string> normalizeSettings(Settings settings,
                                                    const char *executable);
}
