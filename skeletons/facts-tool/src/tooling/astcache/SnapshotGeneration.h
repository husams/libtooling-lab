#pragma once

#include "model/AstCache.h"

#include <string>

namespace facts::astcache::detail {
std::string snapshotGeneration(const Snapshot &snapshot);
} // namespace facts::astcache::detail
