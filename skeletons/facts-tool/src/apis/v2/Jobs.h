#pragma once
#include "apis/runtime/Request.h"

namespace facts::apis::v2 {
domain::Result<runtime::Request> parseJobRequest(std::string operation,
                                                const nlohmann::json &body);
domain::Result<nlohmann::json> executeJob(const domain::Context &context,
                                         const runtime::Request &request);
}
