#pragma once
#include "storage/catalog/Records.h"
#include "storage/catalog/Requests.h"

namespace facts::catalog {
Result<std::vector<Directory>> directories(Database &database,
                                           const std::string &componentName);
Result<Directory> directory(Database &database, const Selector &selector,
                            const std::string &componentName);
Result<void> removeDirectory(Database &database, std::int64_t id);
} // namespace facts::catalog
