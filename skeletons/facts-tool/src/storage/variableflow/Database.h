#pragma once

#include "analysis/variableflow/Model.h"

#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace facts::variableflow {

struct RunMetadata {
  std::string projectPath;
  std::string factsPath;
  std::string functionSelector;
  std::string variableSelector;
  std::vector<std::string> sourceSelectors;
  std::optional<unsigned> declarationLine;
  std::optional<unsigned> maxDepth;
  std::string engine;
  std::string assumptions;
};

std::expected<std::int64_t, std::string>
persist(const std::filesystem::path &path, const RunMetadata &metadata,
        const Graph &graph);

} // namespace facts::variableflow
