#pragma once
#include "apis/watch/ignore/Ignore.h"
#include "apis/watch/ignore/Repository.h"
#include "apis/watch/ignore/Layered.h"

namespace facts::apis::watch {
struct Ignore::Impl {
  std::filesystem::path root;
  std::vector<std::filesystem::path> excludedDirectories;
  ignore::Repository repository{nullptr, git_repository_free};
  ignore::Repository patterns{nullptr, git_repository_free};
  ignore::Index index{nullptr, git_index_free};
  mutable ignore::Layered rules;
};
}
