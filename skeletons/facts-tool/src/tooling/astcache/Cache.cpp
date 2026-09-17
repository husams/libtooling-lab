#include "tooling/astcache/Cache.h"

#include "storage/astcache/Database.h"
#include "tooling/FrontendActivity.h"
#include "tooling/astcache/Includes.h"
#include "tooling/astcache/Metadata.h"
#include "tooling/astcache/Parse.h"
#include "tooling/astcache/Preprocess.h"
#include "tooling/astcache/RevisionObserver.h"
#include "tooling/astcache/Revisions.h"
#include "tooling/astcache/Snapshot.h"
#include "tooling/astcache/Storage.h"

#include <clang/Frontend/ASTUnit.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/Support/raw_ostream.h>

#include <iterator>

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
  reportFrontendActivity(options.verbosity, "ast-parse", source);
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
        const detail::RevisionObservations &observations,
        const IncludeGraphFacts &includes) {
  return detail::captureSnapshot(entry, unit.getSourceManager(),
                                 unit.getPreprocessor(), includes)
      .and_then([&](const Snapshot &snapshot) {
        return detail::validateObservedRevisions(observations)
            .and_then([&]() -> std::expected<void, std::string> {
              if (!detail::currentSnapshot(snapshot))
                return std::unexpected("Git commit changed while capturing AST metadata");
              return detail::storeAST(entry, unit, snapshot)
                  .or_else([&](const std::string &artifactError) {
                    // Dependency metadata remains useful when optional AST
                    // storage is unavailable. Keep the artifact error so the
                    // caller never reports a serialized AST as stored.
                    return storage::astcache::writeSnapshot(entry.database,
                                                            snapshot)
                        .transform_error([&](const std::string &snapshotError) {
                          return artifactError + "; dependency metadata: " +
                                 snapshotError;
                        })
                        .and_then([&]() -> std::expected<void, std::string> {
                          return std::unexpected(artifactError);
                        });
                  });
            });
      });
}

void appendIncludes(IncludeGraphFacts &destination, IncludeGraphFacts source) {
  destination.visitedSources.insert(
      destination.visitedSources.end(),
      std::make_move_iterator(source.visitedSources.begin()),
      std::make_move_iterator(source.visitedSources.end()));
  destination.edges.insert(destination.edges.end(),
                           std::make_move_iterator(source.edges.begin()),
                           std::make_move_iterator(source.edges.end()));
}

int buildOne(const clang::tooling::CompilationDatabase &database,
             const std::string &source,
             std::vector<std::unique_ptr<clang::ASTUnit>> &units,
             const Options &options, clang::DiagnosticConsumer *diagnostics,
             bool clearAdjusters, IncludeGraphFacts *includes = nullptr,
             bool loadCached = true) {
  if (!options.enabled)
    return parse(database, source, units, options, diagnostics, clearAdjusters);
  auto entry = detail::locateEntry(database, source, options, clearAdjusters);
  if (entry && loadCached) {
    IncludeGraphFacts cached;
    if (auto loaded = detail::loadAST(*entry, includes ? &cached : nullptr)) {
      report(options, "hit", source);
      units.push_back(std::move(loaded));
      if (includes)
        appendIncludes(*includes, std::move(cached));
      return 0;
    }
  }
  report(options, "miss", source);
  if (!entry)
    report(options, "unavailable", entry.error());
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
  IncludeGraphFacts parsed;
  if (includes || (entry && status == 0 && units.size() == before + 1))
    for (auto index = before; index < units.size(); ++index) {
      reportFrontendActivity(options.verbosity, "include-reconstruction", source);
      appendIncludes(parsed, includesFromAST(*units[index]));
    }
  if (entry && status == 0 && units.size() == before + 1 &&
      !units.back()->getDiagnostics().hasErrorOccurred() &&
      detail::currentRevisions(revisions)) {
    const auto stored = persist(*entry, *units.back(), observations, parsed);
    if (stored)
      report(options, "stored", source);
    else
      report(options, "unavailable", stored.error());
  }
  if (includes)
    appendIncludes(*includes, std::move(parsed));
  return status;
}

} // namespace

int buildASTs(const clang::tooling::CompilationDatabase &database,
              const std::vector<std::string> &sources,
              std::vector<std::unique_ptr<clang::ASTUnit>> &units,
              const Options &options, clang::DiagnosticConsumer *diagnostics,
              bool clearAdjusters, IncludeGraphFacts *includes) {
  int status = 0;
  for (const auto &source : sources) {
    const auto result = buildOne(database, source, units, options, diagnostics,
                                  clearAdjusters, includes);
    // ClangTool reports parsing failures (1) before skipped inputs (2).
    if (result == 1 || (status == 0 && result != 0))
      status = result;
  }
  return status;
}

int prepareAST(const clang::tooling::CompilationDatabase &database,
               const std::string &source, IncludeGraphFacts &includes,
               const Options &options) {
  if (!options.enabled)
    return preprocess(database, source, includes, options);
  const auto entry = detail::locateEntry(database, source, options);
  if (entry) {
    if (auto prepared = detail::preparedIncludes(*entry)) {
      includes = std::move(*prepared);
      report(options, "hit", source);
      if (options.verbosity >= 1)
        llvm::errs() << "dependency-cache: hit " << source << '\n';
      return 0;
    }
  }
  std::vector<std::unique_ptr<clang::ASTUnit>> units;
  const auto result = buildOne(database, source, units, options, nullptr, false,
                              &includes, false);
  if (result == 0 && !units.empty())
    return 0;
  report(options, "unavailable",
         source + ": AST parse failed; importing dependencies");
  units.clear();
  includes = {};
  return preprocess(database, source, includes, options);
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
