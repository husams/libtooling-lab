#include "commands/match/MatchResultLocation.h"

#include "ast/StoreExtracted.h"
#include "ast/extractors/Location.h"
#include "ast/extractors/NamedDecl.h"

#include <clang/AST/ASTContext.h>
#include <clang/AST/Decl.h>
#include <clang/AST/Stmt.h>
#include <clang/Basic/SourceManager.h>

#include <filesystem>

namespace facts::commands::match {
namespace {
clang::SourceLocation position(const clang::DynTypedNode &node) {
  if (const auto *decl = node.get<clang::Decl>())
    return decl->getLocation();
  return node.getSourceRange().getBegin();
}

ExtractionResult<Location> physicalLocation(
    const clang::SourceManager &source, clang::SourceLocation position) {
  return extractLocation(source, position).transform([&](Location location) {
    const auto expansion = source.getExpansionLoc(position);
    location.line = source.getSpellingLineNumber(expansion);
    location.column = source.getSpellingColumnNumber(expansion);
    return location;
  });
}

llvm::json::Value region(const clang::DynTypedNode &node,
                         const clang::ASTContext &context) {
  const auto &source = context.getSourceManager();
  const auto range = node.getSourceRange();
  const auto begin = source.getExpansionLoc(range.getBegin());
  const auto end = source.getExpansionLoc(range.getEnd());
  if (begin.isInvalid() || end.isInvalid() ||
      source.getFileID(begin) != source.getFileID(end))
    return nullptr;
  const auto extracted = extractRegion(source, context.getLangOpts(), range);
  const auto path = sourcePath(source, begin);
  if (!extracted || path.empty())
    return nullptr;
  return llvm::json::Object{{"path", path}, {"offset", extracted->offset},
                            {"size", extracted->size}};
}
} // namespace

std::string sourcePath(const clang::SourceManager &source,
                       clang::SourceLocation location) {
  location = source.getExpansionLoc(location);
  if (location.isInvalid())
    return {};
  const auto entry = source.getFileEntryRefForID(source.getFileID(location));
  if (!entry)
    return {};
  const auto real = entry->getFileEntry().tryGetRealPathName();
  return real.empty()
             ? std::filesystem::absolute(entry->getName().str())
                   .lexically_normal().string()
             : real.str();
}

llvm::json::Object describeBinding(const clang::DynTypedNode &node,
                                   const clang::ASTContext &context) {
  const auto &source = context.getSourceManager();
  const auto place = physicalLocation(source, position(node));
  const auto path = sourcePath(source, position(node));
  llvm::json::Object binding{{"node_kind", node.getNodeKind().asStringRef()},
                             {"name", nullptr}, {"usr", nullptr},
                             {"location", nullptr}, {"range", region(node, context)},
                             {"location_unavailable_reason", nullptr}};
  if (const auto *decl = node.get<clang::NamedDecl>()) {
    binding["name"] = extractQualifiedName(*decl, source);
    if (auto usr = extractUsr(*decl))
      binding["usr"] = *usr;
  }
  if (place && !path.empty()) {
    binding["location"] = llvm::json::Object{
        {"path", path}, {"line", place->line},
        {"column", place->column}, {"offset", place->offset}};
  } else {
    binding["location_unavailable_reason"] =
        place ? "source-file-unavailable"
              : std::string{extractionErrorName(place.error())};
  }
  return binding;
}

} // namespace facts::commands::match
