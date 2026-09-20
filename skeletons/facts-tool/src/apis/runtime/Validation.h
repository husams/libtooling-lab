#pragma once
#include "apis/runtime/Request.h"
#include <set>

namespace facts::apis::runtime {
domain::Result<void> keys(const Json &value, const std::set<std::string> &allowed);
domain::Result<std::string> text(const Json &value, std::string_view name,
                                std::size_t limit = 4096);
domain::Result<std::string> decodeQuery(std::string_view value);
}
