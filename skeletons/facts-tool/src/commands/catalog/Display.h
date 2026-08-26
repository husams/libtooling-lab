#pragma once
#include "storage/catalog/Records.h"

namespace facts::commands {
catalog::Result<std::string>
displayComponents(const std::vector<catalog::Component> &values);
std::string displayRepositories(const std::vector<catalog::Repository> &values);
std::string displayDirectories(const std::vector<catalog::Directory> &values);
} // namespace facts::commands
