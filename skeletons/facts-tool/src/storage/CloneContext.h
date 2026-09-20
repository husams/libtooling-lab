#pragma once

#include "storage/ProjectConfiguration.h"
#include "model/SymbolId.h"
#include <optional>

namespace facts {

// A native operation may select a registered clone without changing the
// repository's persisted default. The scope stays on its executing thread.
class ScopedCloneContext {
public:
  explicit ScopedCloneContext(std::optional<ProjectClone> clone);
  ~ScopedCloneContext();
  ScopedCloneContext(const ScopedCloneContext &) = delete;
  ScopedCloneContext &operator=(const ScopedCloneContext &) = delete;
  std::span<const FileId> refreshedFiles() const noexcept { return refreshed_; }

private:
  std::optional<ProjectClone> previous_;
  std::vector<FileId> refreshed_;
  std::vector<FileId> *previousMoved_ = nullptr;
};

std::optional<ProjectClone>
invocationClone(std::optional<std::int64_t> repositoryId,
                std::optional<ProjectClone> persistedClone);
std::uint64_t cloneContextRevision() noexcept;
void recordRefreshedCloneFiles(std::span<const FileId> files);

} // namespace facts
