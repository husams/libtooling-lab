#include "tooling/astcache/Includes.h"

#include <clang/Basic/FileEntry.h>
#include <clang/Basic/SourceManager.h>
#include <clang/Frontend/ASTUnit.h>
#include <clang/Lex/PreprocessingRecord.h>
#include <clang/Lex/Preprocessor.h>

#include <filesystem>
#include <optional>

namespace facts::astcache {
namespace {

std::optional<std::string> canonicalPath(clang::FileEntryRef file,
                                         const std::filesystem::path &cwd) {
  const auto real = file.getFileEntry().tryGetRealPathName();
  const auto spelling = real.empty() ? file.getName() : real;
  std::error_code error;
  const auto path = std::filesystem::canonical(cwd / spelling.str(), error);
  if (error)
    return std::nullopt;
  return path.lexically_normal().string();
}

std::optional<std::string> filePath(clang::SourceManager &manager,
                                    clang::FileID file,
                                    const std::filesystem::path &cwd) {
  const auto entry = manager.getFileEntryRefForID(file);
  return entry ? canonicalPath(*entry, cwd) : std::nullopt;
}

void appendInclude(clang::SourceManager &manager,
                   const clang::InclusionDirective &include,
                   const std::filesystem::path &cwd,
                   IncludeGraphFacts &facts) {
  const auto entry = include.getFile();
  if (!entry)
    return;
  const auto location =
      manager.getExpansionLoc(include.getSourceRange().getBegin());
  const auto source = filePath(manager, manager.getFileID(location), cwd);
  const auto destination = canonicalPath(*entry, cwd);
  if (!source || !destination)
    return;
  facts.visitedSources.push_back(*source);
  facts.visitedSources.push_back(*destination);
  facts.edges.push_back({*source, *destination});
}

} // namespace

IncludeGraphFacts includesFromAST(clang::ASTUnit &unit) {
  IncludeGraphFacts facts;
  auto &manager = unit.getSourceManager();
  const std::filesystem::path cwd(unit.getFileSystemOpts().WorkingDir);
  if (const auto main = filePath(manager, manager.getMainFileID(), cwd))
    facts.visitedSources.push_back(*main);
  auto *record = unit.getPreprocessor().getPreprocessingRecord();
  if (!record)
    return facts;
  for (auto *entity : *record)
    if (auto *include = llvm::dyn_cast_or_null<clang::InclusionDirective>(entity))
      appendInclude(manager, *include, cwd, facts);
  return facts;
}

} // namespace facts::astcache
