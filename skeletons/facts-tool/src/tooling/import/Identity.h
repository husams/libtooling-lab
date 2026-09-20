#pragma once
#include "storage/ProjectConfiguration.h"

namespace facts {
std::expected<ProjectConfiguration, std::string>
readImportIdentity(const std::filesystem::path &database, std::int64_t cloneId);
}
