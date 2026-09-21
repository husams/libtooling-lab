#include "tooling/DiagnosticScope.h"
#include "tooling/astcache/Preprocess.h"

#include "storage/astcache/Database.h"
#include "tooling/FrontendActivity.h"
#include "tooling/astcache/Metadata.h"
#include "tooling/astcache/PreprocessAction.h"
#include "tooling/astcache/RevisionObserver.h"
#include "tooling/astcache/Revisions.h"
#include "tooling/astcache/Snapshot.h"

#include <clang/Tooling/Tooling.h>
#include <llvm/Support/VirtualFileSystem.h>
#include <llvm/Support/raw_ostream.h>

namespace facts::astcache {
namespace {

void report(const Options &options, llvm::StringRef event,
            const std::string &detail) {
  if (options.verbosity >= 1)
    llvm::errs() << "dependency-cache: " << event << " " << detail << '\n';
}

void publish(const detail::Entry &entry,
             const detail::CapturedSnapshot &snapshot,
             std::span<const Revision> baseline,
             const detail::RevisionObservations &observations,
             const Options &options) {
  auto saved = snapshot.and_then([&](const Snapshot &value)
                                    -> std::expected<void, std::string> {
    if (!detail::currentRevisions(baseline) || !detail::currentSnapshot(value))
      return std::unexpected("Git HEAD changed while collecting dependencies");
    return detail::validateObservedRevisions(observations).and_then([&] {
      return storage::astcache::writeSnapshot(entry.database, value);
    });
  });
  if (saved)
    report(options, "stored", entry.source.string());
  else
    report(options, "unavailable", saved.error());
}

} // namespace

int preprocess(const clang::tooling::CompilationDatabase &database,
               const std::string &source, IncludeGraphFacts &includes,
               const Options &options) {
  reportFrontendActivity(options.verbosity, "dependency-scan", source);
  // A separate Clang FileManager prevents relative names in different
  // compilation directories from sharing stale file information.
  clang::tooling::ClangTool tool(database, {source},
      std::make_shared<clang::PCHContainerOperations>(),
      llvm::vfs::createPhysicalFileSystem());
  configureDiagnostics(tool);
  if (!options.enabled)
    return tool.run(createIncludeVisitorFactory(includes).get());
  report(options, "miss", source);
  auto entry = detail::locateEntry(database, source, options);
  if (!entry) {
    report(options, "unavailable", entry.error());
    return tool.run(createIncludeVisitorFactory(includes).get());
  }
  const auto baseline = detail::captureRevisions(entry->source, {});
  if (!baseline) {
    report(options, "unavailable", baseline.error());
    return tool.run(createIncludeVisitorFactory(includes).get());
  }
  detail::CapturedSnapshot snapshot =
      std::unexpected("preprocessing did not capture dependency metadata");
  detail::RevisionObservations observations;
  auto factory =
      detail::createPreprocessFactory(*entry, includes, snapshot, observations);
  const int status = tool.run(factory.get());
  if (status == 0)
    publish(*entry, snapshot, *baseline, observations, options);
  return status;
}

} // namespace facts::astcache
