#pragma once
#include "storage/catalog/File.h"
#include <map>

namespace facts::commands::detail {
using CloneAliases = std::map<FileId, std::vector<std::string>>;
std::expected<CloneAliases, std::string>
loadCloneAliases(storage::Database &database, const std::vector<catalog::File> &files);
}
