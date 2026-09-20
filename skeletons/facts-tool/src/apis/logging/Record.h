#pragma once
#include "apis/logging/Level.h"
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>

namespace facts::apis::logging {
std::string record(Level level, std::string_view event, nlohmann::json fields);
}
