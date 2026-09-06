#pragma once

#include "model/Dependency.h"
#include "storage/FactProvenance.h"

#include <expected>
#include <span>
#include <string>
#include <system_error>

namespace facts {

std::expected<void, std::error_code>
replaceDependencies(const std::string &databasePath,
                    std::span<const FileId> visitedSources,
                    std::span<const DependencyEdge> edges,
                    std::span<const storage::FactProvenance> provenance = {});

} // namespace facts
