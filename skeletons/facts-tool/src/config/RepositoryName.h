#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace facts::config {

// A git repository's identity: the name derived from its "origin" remote
// (or, absent that, its alphabetically-first remote), plus that remote's
// URL verbatim.
struct RepositoryIdentity {
  std::string name;
  std::string remoteUrl;
};

// projectRoot must be a git repository's own directory -- no upward search
// is performed. Returns nullopt when projectRoot is not a git repository,
// libgit2 cannot open it (a bare/incomplete ".git", a stray gitlink, etc.),
// it has no remotes, or the chosen remote's URL yields no usable name; the
// caller falls back to the basename (and an empty remote URL) in every one
// of those cases.
std::optional<RepositoryIdentity> repositoryIdentity(const std::filesystem::path &projectRoot);

// The {project_name} identity for a git repository: repositoryIdentity()'s
// name alone. See repositoryIdentity() for the fallback rules.
std::optional<std::string> repositoryName(const std::filesystem::path &projectRoot);

} // namespace facts::config
