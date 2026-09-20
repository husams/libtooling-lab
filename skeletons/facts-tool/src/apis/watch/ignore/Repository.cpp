#include "apis/watch/ignore/Repository.h"
#include "config/GitHandles.h"
#include <git2/sys/repository.h>

namespace facts::apis::watch::ignore {
std::string error(const std::string &operation) {
  const auto *last = git_error_last();
  return operation + ": " + (last && last->message ? last->message : "Git error");
}

std::expected<Repository, std::string>
memoryRepository(const std::filesystem::path &workdir) {
  config::detail::ensureLibgit2Initialized();
  git_repository *raw = nullptr;
  if (git_repository_new(&raw) < 0)
    return std::unexpected(error("cannot create ignore repository"));
  Repository repository(raw, git_repository_free);
  git_config *configuration = nullptr;
  if (git_config_new(&configuration) < 0)
    return std::unexpected(error("cannot create ignore configuration"));
  const int configured = git_repository_set_config(raw, configuration);
  git_config_free(configuration);
  if (configured < 0)
    return std::unexpected(error("cannot attach ignore configuration"));
  // update_gitlink=0 keeps this fallback entirely in memory: no .git is created.
  if (!workdir.empty() && git_repository_set_workdir(raw, workdir.c_str(), 0) < 0)
    return std::unexpected(error("cannot set ignore working directory"));
  return repository;
}

std::expected<Repository, std::string>
openRepository(const std::filesystem::path &root) {
  config::detail::ensureLibgit2Initialized();
  git_repository *raw = nullptr;
  const int opened = git_repository_open_ext(
      &raw, root.c_str(), GIT_REPOSITORY_OPEN_NO_SEARCH, nullptr);
  if (opened == GIT_ENOTFOUND) return memoryRepository(root);
  if (opened < 0)
    return std::unexpected(error("cannot open clone " + root.string()));
  return Repository(raw, git_repository_free);
}

std::expected<bool, std::string>
matches(git_repository *repository, const std::string &relative) {
  int ignored = 0;
  if (git_ignore_path_is_ignored(&ignored, repository, relative.c_str()) < 0)
    return std::unexpected(error("cannot check ignore rules for " + relative));
  return ignored != 0;
}
}
