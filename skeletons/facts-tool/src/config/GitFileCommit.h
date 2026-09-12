#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>

namespace facts::config {

// The HEAD commit of the git repository that tracks a file, memoized per
// directory and per repository so a caller resolving many files under the
// same project only walks the filesystem and reads HEAD once. Not tied to
// libgit2 types in the header, the way RepositoryName.h keeps git2.h out of
// its own interface.
class GitCommitResolver {
public:
  GitCommitResolver();
  ~GitCommitResolver();

  GitCommitResolver(GitCommitResolver &&) noexcept;
  GitCommitResolver &operator=(GitCommitResolver &&) noexcept;
  GitCommitResolver(const GitCommitResolver &) = delete;
  GitCommitResolver &operator=(const GitCommitResolver &) = delete;

  // The 40-hex HEAD commit of the repository tracking `file`, or nullopt when
  // `file` is untracked, ignored, outside every repository, or HEAD is
  // unborn. `file` is used as spelled -- pass the registry identity path
  // (symlinks intact), not a canonicalized one, so a generated source
  // symlinked into the project resolves the project's own repository.
  std::optional<std::string> commitFor(const std::filesystem::path &file);

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace facts::config
