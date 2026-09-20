#pragma once
#include "apis/config/Settings.h"
#include <expected>

namespace facts::apis {
std::expected<Settings, int> parseSettings(int argc, char **argv);
}
