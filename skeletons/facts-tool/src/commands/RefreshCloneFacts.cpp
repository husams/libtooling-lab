#include "commands/RefreshCloneFacts.h"
#include "storage/FactStore.h"
#include "storage/FileManager.h"

namespace facts::commands {
std::expected<void, std::string> refreshCloneFacts(
    FactStore &store, const FactPairProvenanceSnapshot &snapshot,
    FileManager &registry, const std::vector<std::string> &files) {
  std::vector<FileId> selected;
  for (const auto &file : files) {
    auto id = registry.getId(file);
    if (!id) return std::unexpected("cannot resolve clone refresh source: " + file);
    selected.push_back(*id);
  }
  return store.refreshCloneFiles(snapshot, selected).transform_error([](auto error) {
    return "cannot refresh moved clone facts: " + error.message();
  });
}
}
