#include "apis/watch/ignore/State.h"

namespace facts::apis::watch {
std::vector<std::filesystem::path> Ignore::controlFiles() const {
  std::vector<std::filesystem::path> result;
  if (!git_repository_path(impl_->repository.get())) return result;
  for (const auto item : {GIT_REPOSITORY_ITEM_INDEX, GIT_REPOSITORY_ITEM_INFO,
                          GIT_REPOSITORY_ITEM_CONFIG,
                          GIT_REPOSITORY_ITEM_WORKTREE_CONFIG}) {
    git_buf buffer{};
    if (git_repository_item_path(&buffer, impl_->repository.get(), item) == 0) {
      auto path = std::filesystem::path(buffer.ptr);
      if (item == GIT_REPOSITORY_ITEM_INFO) path /= "exclude";
      result.push_back(path.lexically_normal());
    }
    git_buf_dispose(&buffer);
  }
  git_config *configuration = nullptr;
  if (git_repository_config(&configuration, impl_->repository.get()) == 0) {
    git_buf buffer{};
    if (git_config_get_path(&buffer, configuration, "core.excludesfile") == 0)
      result.emplace_back(buffer.ptr);
    git_buf_dispose(&buffer);
    git_config_free(configuration);
  }
  return result;
}
}
