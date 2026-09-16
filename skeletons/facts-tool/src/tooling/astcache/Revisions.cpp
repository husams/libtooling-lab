#include "tooling/astcache/Revisions.h"

#include "config/GitFileCommit.h"
#include "config/GitHandles.h"

#include <algorithm>
#include <map>
#include <optional>

namespace facts::astcache::detail {
std::optional<Revision>
readRepositoryRevision(const std::filesystem::path &directory, bool search) {
  config::detail::ensureLibgit2Initialized();
  config::detail::RepositoryHandle repository;
  const auto flags = search ? 0U : GIT_REPOSITORY_OPEN_NO_SEARCH;
  auto location = directory;
  if (search) {
    std::error_code error;
    while (!std::filesystem::is_directory(location, error) && location.has_relative_path()) {
      location = location.parent_path();
      error.clear();
    }
  }
  if (git_repository_open_ext(&repository.repo, location.c_str(), flags,
                              nullptr) != 0)
    return std::nullopt;
  const auto *workdir = git_repository_workdir(repository.repo);
  if (!workdir)
    return std::nullopt;
  auto root = std::filesystem::path(workdir).lexically_normal();
  if (root.has_relative_path() && root.filename().empty())
    root = root.parent_path();
  git_oid commit;
  if (git_reference_name_to_id(&commit, repository.repo, "HEAD") != 0)
    return Revision{root.string(), ""};
  char hash[GIT_OID_HEXSZ + 1];
  git_oid_tostr(hash, sizeof(hash), &commit);
  return Revision{root.string(), hash};
}

std::expected<std::vector<Revision>, std::string>
captureRevisions(const std::filesystem::path &source,
                 std::span<const Input> inputs,
                 std::span<const std::filesystem::path> searchDirectories) {
  config::GitCommitResolver resolver;
  const auto sourceCommit = resolver.commitFor(source);
  if (!sourceCommit)
    return std::unexpected("AST cache requires a Git-tracked source with a HEAD commit");
  const auto sourceRevision = readRepositoryRevision(source.parent_path(), true);
  if (!sourceRevision || sourceRevision->commit != *sourceCommit)
    return std::unexpected("Git HEAD changed while collecting dependencies");
  std::map<std::string, Revision> repositories{{sourceRevision->path, *sourceRevision}};
  std::map<std::filesystem::path, std::optional<Revision>> directories{
      {source.parent_path(), sourceRevision}};
  const auto record = [&](const std::filesystem::path &directory)
      -> std::expected<void, std::string> {
    auto [position, inserted] = directories.try_emplace(directory);
    if (inserted)
      position->second = readRepositoryRevision(directory, true);
    const auto &revision = position->second;
    if (revision) {
      const auto [saved, first] = repositories.try_emplace(revision->path, *revision);
      if (!first && saved->second != *revision)
        return std::unexpected("Git HEAD changed while collecting dependencies");
    }
    return {};
  };
  for (const auto &input : inputs)
    if (auto result = record(std::filesystem::path(input.path).parent_path()); !result)
      return std::unexpected(result.error());
  for (const auto &directory : searchDirectories)
    if (auto result = record(directory); !result)
      return std::unexpected(result.error());
  std::vector<Revision> revisions;
  for (const auto &[path, revision] : repositories)
    revisions.push_back(revision);
  return revisions;
}

bool currentRevisions(std::span<const Revision> revisions) {
  return !revisions.empty() &&
         std::ranges::all_of(revisions, [](const Revision &saved) {
           const auto current = readRepositoryRevision(saved.path, false);
           return current && *current == saved;
         });
}
} // namespace facts::astcache::detail
