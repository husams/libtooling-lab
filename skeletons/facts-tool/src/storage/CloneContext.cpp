#include "storage/CloneContext.h"
#include <algorithm>
#include <utility>

namespace facts {
namespace {
thread_local std::optional<ProjectClone> selectedClone;
thread_local std::uint64_t revision = 0;
thread_local std::vector<FileId> *movedFiles = nullptr;
}

ScopedCloneContext::ScopedCloneContext(std::optional<ProjectClone> clone)
    : previous_(std::exchange(selectedClone, std::move(clone))),
      previousMoved_(std::exchange(movedFiles, &refreshed_)) {
  ++revision;
}

ScopedCloneContext::~ScopedCloneContext() {
  selectedClone = std::move(previous_);
  movedFiles = previousMoved_;
  ++revision;
}

std::optional<ProjectClone>
invocationClone(std::optional<std::int64_t> repositoryId,
                std::optional<ProjectClone> persistedClone) {
  return selectedClone && repositoryId == selectedClone->repositoryId
             ? selectedClone
             : std::move(persistedClone);
}
std::uint64_t cloneContextRevision() noexcept { return revision; }
void recordRefreshedCloneFiles(std::span<const FileId> files) {
  if (!movedFiles) return;
  movedFiles->insert(movedFiles->end(), files.begin(), files.end());
  std::ranges::sort(*movedFiles);
  movedFiles->erase(std::ranges::unique(*movedFiles).begin(), movedFiles->end());
}
} // namespace facts
