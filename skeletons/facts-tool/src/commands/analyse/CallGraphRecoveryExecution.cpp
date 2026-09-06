#include "commands/analyse/CallGraphRecoveryInternal.h"
#include "commands/analyse/RecoveryCompilation.h"

#include "ast/FactExtractor.h"
#include "ast/Indexing.h"
#include "commands/ExtractionSetup.h"
#include "commands/FactPairValidation.h"
#include "platform/PlatformFlags.h"
#include "storage/FactStore.h"
#include "storage/FileManager.h"
#include "tooling/StoredCompilationDatabase.h"

#include <clang/Tooling/Tooling.h>

namespace facts::commands {

std::expected<RecoveryAttemptResult, std::string>
extractRecoveryCandidate(const RecoveryContext &context,
                         const cli::CallGraphOptions &options,
                         const RecoveryCandidate &candidate) {
  const std::vector<std::string> sources{candidate.source.string()};
  if (candidate.entry.arguments.empty())
    return std::unexpected(candidate.entry.reason);
  RecoveryCompilation database(candidate);
  auto files = FileManager::openReadOnly(context.project, options.verbosity);
  if (!files)
    return std::unexpected(files.error());
  auto pairing = prepareFactPairForWrite(options.facts, context.project);
  if (!pairing)
    return std::unexpected(pairing.error());
  FactStore store(options.facts, options.verbosity);
  if (auto begun = store.begin(); !begun)
    return std::unexpected("cannot begin recovery transaction: " +
                           begun.error().message());
  IndexingStatus status;
  clang::tooling::ClangTool tool(database, sources);
  tool.clearArgumentsAdjusters();
  const int result =
      tool.run(createFactExtractorFactory(**files, store, status).get());
  if (result != 0 || !status.complete()) {
    (void)store.rollback();
    return RecoveryAttemptResult{false, result != 0
                                          ? "compiler returned " +
                                                std::to_string(result)
                                          : "indexing incomplete"};
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
  if (auto ended = store.end(); !ended)
    return std::unexpected("cannot commit recovery transaction: " +
                           ended.error().message());
  return RecoveryAttemptResult{true, "recovered"};
}

} // namespace facts::commands
