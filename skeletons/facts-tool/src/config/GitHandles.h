#pragma once

#include <git2.h>

#include <cstdlib>

namespace facts::config::detail {

// libgit2 requires exactly one git_libgit2_init() per process (calls nest,
// but every init needs a matching shutdown); a function-local static runs
// it exactly once regardless of how many repositories this process opens,
// and registers the matching shutdown at exit.
inline void ensureLibgit2Initialized() {
  static const bool initialized = [] {
    git_libgit2_init();
    std::atexit([] { git_libgit2_shutdown(); });
    return true;
  }();
  (void)initialized;
}

struct RepositoryHandle {
  git_repository *repo = nullptr;
  ~RepositoryHandle() { git_repository_free(repo); }
};

struct RemoteHandle {
  git_remote *remote = nullptr;
  ~RemoteHandle() { git_remote_free(remote); }
};

struct StringArrayHandle {
  git_strarray array{};
  ~StringArrayHandle() { git_strarray_dispose(&array); }
};

} // namespace facts::config::detail
