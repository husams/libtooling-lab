#pragma once
#include "apis/http/Router.h"
#include "apis/domain/Selection.h"

namespace facts::apis {
Response resourceError(const domain::Error &error);
}
