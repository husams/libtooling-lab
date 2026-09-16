#pragma once

#include "model/AstCache.h"

#include <expected>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace facts::astcache::detail {
std::optional<Revision>
readRepositoryRevision(const std::filesystem::path &directory, bool search);
std::expected<std::vector<Revision>, std::string>
captureRevisions(const std::filesystem::path &source,
                 std::span<const Input> inputs,
                 std::span<const std::filesystem::path> searchDirectories = {});
bool currentRevisions(std::span<const Revision> revisions);
} // namespace facts::astcache::detail
