#pragma once
#include "apis/http/Router.h"

namespace facts::apis {
std::expected<void, HttpError> authorize(const Request &request, const Settings &settings);
}
