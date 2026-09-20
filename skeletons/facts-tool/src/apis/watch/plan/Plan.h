#pragma once
#include "apis/watch/catalog/Catalog.h"

namespace facts::apis::watch {
struct Plan {
  std::vector<std::vector<std::string>> imports;
  std::vector<std::vector<std::string>> extracts;
};
std::expected<std::vector<std::filesystem::path>, std::string>
explicitDatabases(const Settings &settings);
std::expected<Plan, std::string> buildPlan(
    const Settings &settings, const Catalog &catalog,
    const std::vector<std::filesystem::path> &compilationDirectories);
}
