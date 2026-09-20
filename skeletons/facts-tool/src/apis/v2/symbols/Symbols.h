#pragma once
#include "apis/domain/Selection.h"
#include <map>
#include <string_view>

namespace facts::apis::v2::symbols {
using Json = nlohmann::json;
using QueryParameters = std::map<std::string, std::string>;
domain::Result<Json> search(const domain::Context &, const QueryParameters &);
domain::Result<Json> read(const domain::Context &, std::string_view id,
                          std::string_view child, const QueryParameters &);
}
