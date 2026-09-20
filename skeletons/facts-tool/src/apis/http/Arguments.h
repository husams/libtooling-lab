#pragma once
#include "apis/jobs/Queue.h"
#include <expected>

namespace facts::apis {
std::expected<std::vector<std::string>, std::string>
arguments(const Json &body, const std::string &path,
          const std::vector<std::string> &commands, const Settings &settings);
}
