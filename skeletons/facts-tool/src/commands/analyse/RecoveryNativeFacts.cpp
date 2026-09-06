#include "ast/Indexing.h"
#include "ast/visitors/Traversal.h"
#include "commands/analyse/CallGraphRecoveryInternal.h"
#include "commands/analyse/RecoveryScan.h"
#include "storage/FactStore.h"
#include "storage/FileManager.h"
#include <llvm/Support/FileSystem.h>

namespace facts::commands {
namespace {
struct ScratchFacts {
  llvm::SmallString<128> directory;

  ~ScratchFacts() {
    if (!directory.empty())
      (void)llvm::sys::fs::remove_directories(directory);
  }
};
} // namespace

callgraph::QueryResult
collectRecoveryNativeFacts(const RecoveryContext &context,
                           const RecoveryScan &scan) {
  if (scan.status != 0)
    return std::unexpected("evidence validation compiler failure");
  ScratchFacts scratch;
  if (auto error = llvm::sys::fs::createUniqueDirectory("facts-recovery",
                                                        scratch.directory))
    return std::unexpected("cannot create recovery evidence store: " +
                           error.message());
  const auto path =
      (std::filesystem::path(scratch.directory.str().str()) / "facts.db")
          .string();
  auto files = FileManager::openReadOnly(context.project);
  if (!files)
    return std::unexpected(files.error());
  try {
    FactStore store(path);
    if (auto begun = store.begin(); !begun)
      return std::unexpected(begun.error().message());
    IndexingStatus status;
    for (const auto &unit : scan.units)
      traverse(unit->getASTContext(), **files, store, status);
    if (!status.complete()) {
      (void)store.rollback();
      return std::unexpected("native body evidence collection incomplete");
    }
    if (auto ended = store.end(); !ended)
      return std::unexpected(ended.error().message());
    return callgraph::loadCallGraph(path);
  } catch (const std::exception &error) {
    return std::unexpected(error.what());
  }
}
} // namespace facts::commands
