#include "tooling/astcache/Cache.h"

#include "tooling/astcache/Includes.h"
#include "tooling/astcache/Metadata.h"
#include "tooling/astcache/Parse.h"
#include "tooling/astcache/RevisionObserver.h"
#include "tooling/astcache/Revisions.h"
#include "tooling/astcache/Snapshot.h"
#include "tooling/astcache/Storage.h"

#include <clang/Frontend/ASTUnit.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/Support/raw_ostream.h>

namespace facts::astcache {
namespace {

void report(const Options &options, llvm::StringRef event,
            const std::string &source) {
  if (options.verbosity >= 1)
    llvm::errs() << "ast-cache: " << event << " " << source << '\n';
}

int parse(const clang::tooling::CompilationDatabase &database,
          const std::string &source,
          std::vector<std::unique_ptr<clang::ASTUnit>> &units,
          const Options &options, clang::DiagnosticConsumer *diagnostics,
          bool clearAdjusters,
          detail::RevisionObservations *observations = nullptr) {
  clang::tooling::ClangTool tool(database, {source});
  if (clearAdjusters)
    tool.clearArgumentsAdjusters();
  if (diagnostics)
    tool.setDiagnosticConsumer(diagnostics);
  return options.enabled ? detail::parsePersistent(tool, units, observations)
                         : tool.buildASTs(units);
}

std::expected<void, std::string>
persist(const detail::Entry &entry, clang::ASTUnit &unit,
        const detail::RevisionObservations &observations) {
  return detail::captureSnapshot(entry, unit.getSourceManager(),
                                 unit.getPreprocessor(), includesFromAST(unit))
      .and_then([&](const Snapshot &snapshot) {
        return detail::validateObservedRevisions(observations)
            .and_then([&]() -> std::expected<void, std::string> {
              if (!detail::currentSnapshot(snapshot))
                return std::unexpected("Git commit changed while capturing AST metadata");
              return detail::storeAST(entry, unit, snapshot);
            });
      });
}

int buildOne(const clang::tooling::CompilationDatabase &database,
             const std::string &source,
             std::vector<std::unique_ptr<clang::ASTUnit>> &units,
             const Options &options, clang::DiagnosticConsumer *diagnostics,
             bool clearAdjusters) {
  if (!options.enabled)
    return parse(database, source, units, options, diagnostics, clearAdjusters);
  auto entry = detail::locateEntry(database, source, options, clearAdjusters);
  if (entry) {
    if (auto loaded = detail::loadAST(*entry)) {
      report(options, "hit", source);
      units.push_back(std::move(loaded));
      return 0;
    }
  }
  report(options, "miss", source);
  std::vector<Revision> revisions;
  if (entry) {
    auto initial = detail::captureRevisions(entry->source, {});
    if (initial)
      revisions = std::move(*initial);
    else {
      report(options, "unavailable", initial.error());
      entry = std::unexpected(initial.error());
    }
  }
  detail::RevisionObservations observations;
  const auto before = units.size();
  const int status = parse(database, source, units, options, diagnostics,
                           clearAdjusters, entry ? &observations : nullptr);
  if (entry && status == 0 && units.size() == before + 1 &&
      !units.back()->getDiagnostics().hasErrorOccurred() &&
      detail::currentRevisions(revisions)) {
    const auto stored = persist(*entry, *units.back(), observations);
    if (stored)
      report(options, "stored", source);
    else
      report(options, "unavailable", stored.error());
  }
  return status;
}

} // namespace

int buildASTs(const clang::tooling::CompilationDatabase &database,
              const std::vector<std::string> &sources,
              std::vector<std::unique_ptr<clang::ASTUnit>> &units,
              const Options &options, clang::DiagnosticConsumer *diagnostics,
              bool clearAdjusters) {
  int status = 0;
  for (const auto &source : sources) {
    const auto result = buildOne(database, source, units, options, diagnostics,
                                  clearAdjusters);
    // ClangTool reports parsing failures (1) before skipped inputs (2).
    if (result == 1 || (status == 0 && result != 0))
      status = result;
  }
  return status;
}

std::optional<IncludeGraphFacts>
cachedIncludes(const clang::tooling::CompilationDatabase &database,
               const std::string &source, const Options &options) {
  if (!options.enabled)
    return std::nullopt;
  const auto entry = detail::locateEntry(database, source, options);
  if (!entry)
    return std::nullopt;
  const auto snapshot = detail::readCurrentSnapshot(*entry);
  if (!snapshot || !*snapshot)
    return std::nullopt;
  if (options.verbosity >= 1)
    llvm::errs() << "dependency-cache: hit " << source << '\n';
  return detail::includesFromSnapshot(**snapshot);
}

} // namespace facts::astcache
