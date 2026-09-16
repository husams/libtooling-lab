#pragma once

#include "model/AstCache.h"

#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace facts::storage::astcache {

std::expected<std::optional<facts::astcache::Snapshot>, std::string>
readSnapshot(const std::filesystem::path &project, std::string_view key);

std::expected<void, std::string>
writeSnapshot(const std::filesystem::path &project,
              const facts::astcache::Snapshot &snapshot);

std::expected<std::optional<facts::astcache::Artifact>, std::string>
readArtifact(const std::filesystem::path &project, std::string_view key);

std::expected<void, std::string>
writeArtifact(const std::filesystem::path &project,
              const facts::astcache::Snapshot &snapshot,
              const facts::astcache::Artifact &artifact);

} // namespace facts::storage::astcache
