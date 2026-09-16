#include "tooling/astcache/Cache.h"

#include "tooling/astcache/Includes.h"
#include "tooling/astcache/Metadata.h"
#include "tooling/astcache/Parse.h"
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
          bool clearAdjusters, std::vector<std::string> &lookupNames) {
  clang::tooling::ClangTool tool(database, {source});
  if (clearAdjusters)
    tool.clearArgumentsAdjusters();
  if (diagnostics)
    tool.setDiagnosticConsumer(diagnostics);
  if (options.enabled)
    return detail::parseWithLookups(tool, units, lookupNames);
  return tool.buildASTs(units);
}

int buildOne(const clang::tooling::CompilationDatabase &database,
             const std::string &source,
             std::vector<std::unique_ptr<clang::ASTUnit>> &units,
             const Options &options, clang::DiagnosticConsumer *diagnostics,
             bool clearAdjusters) {
  std::vector<std::string> lookupNames;
  if (!options.enabled)
    return parse(database, source, units, options, diagnostics, clearAdjusters,
                 lookupNames);
  auto entry = detail::locateEntry(database, source, options.directory,
                                    clearAdjusters);
  if (entry) {
    if (auto loaded = detail::loadAST(*entry)) {
      report(options, "hit", source);
      units.push_back(std::move(loaded));
      return 0;
    }
  }
  report(options, "miss", source);
  const auto before = units.size();
  const int status = parse(database, source, units, options, diagnostics,
                           clearAdjusters, lookupNames);
  if (entry && status == 0 && units.size() == before + 1 &&
      !units.back()->getDiagnostics().hasErrorOccurred() &&
      units.back()->getDiagnostics().getNumWarnings() == 0) {
    entry->lookup_names = std::move(lookupNames);
    const auto stored = detail::storeAST(*entry, *units.back());
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
  const auto entry = detail::locateEntry(database, source, options.directory);
  if (!entry)
    return std::nullopt;
  auto unit = detail::loadAST(*entry);
  if (!unit)
    return std::nullopt;
  report(options, "hit", source);
  return includesFromAST(*unit);
}

} // namespace facts::astcache
