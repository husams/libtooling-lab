#include "commands/analyse/CallGraphRecoveryInternal.h"
#include "commands/analyse/RecoveryScan.h"

#include "ast/Indexing.h"
#include "ast/visitors/Traversal.h"
#include "commands/FactPairValidation.h"
#include "storage/FactStore.h"
#include "storage/FileManager.h"

namespace facts::commands {

std::expected<RecoveryAttemptResult, std::string> extractRecoveryCandidate(
    const RecoveryContext &context, const cli::CallGraphOptions &options,
    const RecoveryCandidate &candidate, const RecoveryEvidence *retained) {
  if (candidate.entry.arguments.empty())
    return std::unexpected(candidate.entry.reason);
  const auto scan = context.scans.find(candidate.entry.tuFileId);
  if (scan == context.scans.end() || scan->second->status != 0 ||
      scan->second->units.empty())
    return std::unexpected("missing parsed recovery translation unit");
  auto files = FileManager::openReadOnly(context.project, options.verbosity);
  if (!files)
    return std::unexpected(files.error());
  auto pairing = prepareFactPairForWrite(options.facts, context.project);
  if (!pairing)
    return std::unexpected(pairing.error());
  // Extraction summaries, coverage notices and indexing diagnostics are
  // verbose-only inside a graph run; the run tables keep the durable form.
  const bool verbose = options.verbosity >= 1;
  FactStore store(options.facts, verbose ? options.verbosity : -1);
  if (auto begun = store.begin(); !begun)
    return std::unexpected("cannot begin recovery transaction: " +
                           begun.error().message());
  IndexingStatus status{verbose};
  for (const auto &unit : scan->second->units)
    traverse(unit->getASTContext(), **files, store, status);
  if (!status.complete()) {
    (void)store.rollback();
    return RecoveryAttemptResult{false, "indexing incomplete"};
  }
  auto id = (*files)->getId(candidate.source.string());
  if (!id) {
    (void)store.rollback();
    return std::unexpected("cannot resolve recovered source: " +
                           id.error().message());
  }
  const std::vector<FileId> selected{*id};
  if (auto registered = registerFactPairProvenance(store, *pairing, selected);
      !registered) {
    (void)store.rollback();
    return std::unexpected(registered.error());
  }
  if (retained && !retained->entries.empty()) {
    if (auto published = publishRecoveryEvidence(store, *retained);
        !published) {
      (void)store.rollback();
      return std::unexpected("cannot republish recovery evidence: " +
                             published.error().message());
    }
  }
  if (auto ended = store.end(verbose); !ended)
    return std::unexpected("cannot commit recovery transaction: " +
                           ended.error().message());
  return RecoveryAttemptResult{true, "recovered"};
}

} // namespace facts::commands
