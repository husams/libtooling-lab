#include "config/GitFileCommit.h"
#include "config/GitHandles.h"

#include <git2.h>

#include <unordered_map>
#include <utility>

namespace facts::config {
namespace {

// git_repository_workdir() answers with a trailing separator; strip it so the
// same string works both as a cache key and as the base of a lexically
// relative path.
std::string normalizedWorkdir(const char *raw) {
  std::string value(raw);
  while (!value.empty() && value.back() == '/') {
    value.pop_back();
  }
  return value;
}

std::optional<std::string> headCommit(git_repository *repository) {
  git_oid oid;
  if (git_reference_name_to_id(&oid, repository, "HEAD") != 0) {
    return std::nullopt;
  }
  const auto *hex = git_oid_tostr_s(&oid);
  return hex ? std::optional<std::string>(hex) : std::nullopt;
}

} // namespace

struct GitCommitResolver::Impl {
  // What a directory's repository is: the workdir path, or nullopt when the
  // directory is outside every repository (a bare repository counts as
  // outside one too, since a bare repository tracks nothing on disk).
  std::unordered_map<std::string, std::optional<std::string>>
      workdirByDirectory;

  struct RepositoryState {
    detail::RepositoryHandle repository;
    detail::IndexHandle index;
    std::optional<std::string> headHex;
  };

  // One entry per repository, keyed by workdir, so two files under the same
  // project share one open repository, one index, and one HEAD lookup.
  std::unordered_map<std::string, RepositoryState> stateByWorkdir;
};

GitCommitResolver::GitCommitResolver() : impl_(std::make_unique<Impl>()) {}

GitCommitResolver::~GitCommitResolver() = default;
GitCommitResolver::GitCommitResolver(GitCommitResolver &&) noexcept = default;
GitCommitResolver &
GitCommitResolver::operator=(GitCommitResolver &&) noexcept = default;

std::optional<std::string>
GitCommitResolver::commitFor(const std::filesystem::path &file) {
  detail::ensureLibgit2Initialized();
  const auto directory = file.parent_path().string();

  auto located = impl_->workdirByDirectory.find(directory);
  if (located == impl_->workdirByDirectory.end()) {
    // Open with search enabled and no ceiling: `directory` may be a
    // subdirectory of the repository's working tree, not the tree's root.
    detail::RepositoryHandle candidate;
    std::optional<std::string> workdir;
    if (git_repository_open_ext(&candidate.repo, directory.c_str(), 0,
                                nullptr) == 0) {
      if (const auto *raw = git_repository_workdir(candidate.repo)) {
        workdir = normalizedWorkdir(raw);
      }
    }
    if (workdir && !impl_->stateByWorkdir.contains(*workdir)) {
      auto [state, inserted] = impl_->stateByWorkdir.try_emplace(*workdir);
      (void)inserted;
      // Ownership moves from the local RAII handle to the cached one; null
      // the source so its destructor does not free what the cache now owns.
      state->second.repository.repo = candidate.repo;
      candidate.repo = nullptr;
      state->second.headHex = headCommit(state->second.repository.repo);
      git_repository_index(&state->second.index.index,
                           state->second.repository.repo);
    }
    located =
        impl_->workdirByDirectory.try_emplace(directory, std::move(workdir))
            .first;
  }

  if (!located->second) {
    return std::nullopt;
  }

  auto state = impl_->stateByWorkdir.find(*located->second);
  if (state == impl_->stateByWorkdir.end() ||
      state->second.index.index == nullptr) {
    return std::nullopt;
  }

  const auto tracked = [&](const std::filesystem::path &candidate) {
    const auto relative =
        candidate.lexically_relative(*located->second).generic_string();
    return git_index_get_bypath(state->second.index.index, relative.c_str(),
                                0) != nullptr;
  };

  // Try the lexical (symlink-intact) path against the index first: the
  // registry identity spelling is what a tracked path actually is when the
  // path itself is a symlink (a symlink is its own blob at its own path in
  // the index, and resolving it first would look up the wrong entry, or
  // one that is not tracked at all). The workdir libgit2 reports is a real
  // (symlink-resolved) path, though, so a purely lexical relative path only
  // lines up with it when `file` reaches the workdir through no symlinked
  // intermediate directory; fall back to the canonical path so a source
  // reached through a symlinked intermediate directory still lands on its
  // real, tracked index entry instead of missing outright.
  if (tracked(file)) {
    return state->second.headHex;
  }
  std::error_code canonicalError;
  const auto canonicalFile =
      std::filesystem::weakly_canonical(file, canonicalError);
  if (!canonicalError && tracked(canonicalFile)) {
    return state->second.headHex;
  }
  return std::nullopt; // not tracked
}

} // namespace facts::config
