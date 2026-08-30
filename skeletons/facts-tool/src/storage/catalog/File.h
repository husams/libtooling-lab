#pragma once

#include "storage/catalog/Records.h"

namespace facts::catalog {

Result<std::vector<File>> files(Database &database);
Result<File> file(Database &database, const std::string &path);
Result<void> addFile(Database &database, const std::string &path,
                     const std::string &driver,
                     const std::string &workingDirectory,
                     const std::string &compileOptions);
Result<void> removeFile(Database &database, std::int64_t id);
Result<void> setFileCompileOptions(Database &database, std::int64_t id,
                                   const std::string &compileOptions);

} // namespace facts::catalog
