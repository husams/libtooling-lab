#pragma once
#include "apis/jobs/Queue.h"

namespace facts::apis {
Json openApi(const std::vector<std::string> &commands, bool authenticated);
}
