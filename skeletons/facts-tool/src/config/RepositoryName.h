#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace facts::config {

// The {project_name} identity for a git repository: the name derived from
// its "origin" remote, or (absent that) its alphabetically-first remote.
// projectRoot must be a git repository's own directory -- no upward search
// is performed. Returns nullopt when projectRoot is not a git repository,
// libgit2 cannot open it (a bare/incomplete ".git", a stray gitlink, etc.),
// it has no remotes, or the chosen remote's URL yields no usable name; the
// caller falls back to the basename in every one of those cases.
std::optional<std::string> repositoryName(const std::filesystem::path &projectRoot);

} // namespace facts::config
