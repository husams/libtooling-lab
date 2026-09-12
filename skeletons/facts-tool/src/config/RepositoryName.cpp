#include "config/RepositoryName.h"
#include "config/GitHandles.h"
#include "config/RepositoryUrl.h"

#include <git2.h>

#include <algorithm>
#include <vector>

namespace facts::config {
namespace {

// The remote whose URL supplies {project_name}: "origin" when the
// repository has one, otherwise the alphabetically first remote name.
// nullopt when the repository has no remotes, or the chosen remote/its URL
// cannot be read.
std::optional<std::string> preferredRemoteUrl(git_repository *repo) {
  detail::StringArrayHandle remotes;
  if (git_remote_list(&remotes.array, repo) != 0 || remotes.array.count == 0) return std::nullopt;
  std::vector<std::string> names(remotes.array.strings, remotes.array.strings + remotes.array.count);
  const auto hasOrigin = std::find(names.begin(), names.end(), std::string("origin")) != names.end();
  const auto &chosen = hasOrigin ? std::string("origin") : *std::min_element(names.begin(), names.end());
  detail::RemoteHandle remote;
  if (git_remote_lookup(&remote.remote, repo, chosen.c_str()) != 0) return std::nullopt;
  const auto *url = git_remote_url(remote.remote);
  return url ? std::optional<std::string>(url) : std::nullopt;
}

} // namespace

std::optional<RepositoryIdentity> repositoryIdentity(const std::filesystem::path &projectRoot) {
  std::error_code error;
  if (!std::filesystem::exists(projectRoot / ".git", error) || error) return std::nullopt;
  detail::ensureLibgit2Initialized();
  detail::RepositoryHandle handle;
  // GIT_REPOSITORY_OPEN_NO_SEARCH: {project_name} names the project root's
  // own repository, never an ancestor's -- detail::projectRoot() already
  // walked up to find it, so opening must not walk further.
  if (git_repository_open_ext(&handle.repo, projectRoot.c_str(), GIT_REPOSITORY_OPEN_NO_SEARCH,
                              nullptr) != 0)
    return std::nullopt;
  const auto url = preferredRemoteUrl(handle.repo);
  if (!url) return std::nullopt;
  auto name = repositoryNameFromUrl(*url);
  if (name.empty()) return std::nullopt;
  return RepositoryIdentity{.name = std::move(name), .remoteUrl = *url};
}

std::optional<std::string> repositoryName(const std::filesystem::path &projectRoot) {
  auto identity = repositoryIdentity(projectRoot);
  if (!identity) return std::nullopt;
  return std::move(identity->name);
}

} // namespace facts::config
