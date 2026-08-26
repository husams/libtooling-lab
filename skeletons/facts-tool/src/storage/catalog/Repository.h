#pragma once
#include "storage/catalog/Records.h"

namespace facts::catalog {
Result<std::vector<Repository>> repositories(Database &database);
Result<Repository> repository(Database &database, const std::string &name);
Result<std::vector<ProjectClone>> clones(Database &database,
                                         std::int64_t repositoryId);
Result<void> addClone(Database &database, const Repository &repository,
                      const std::string &path, const std::string &label);
Result<void> switchClone(Database &database, const Repository &repository,
                         const std::string &target);
Result<void> removeRepository(Database &database, const Repository &repository,
                              bool deleteComponents);
} // namespace facts::catalog
