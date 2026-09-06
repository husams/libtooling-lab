#include "commands/match/SymbolDispatch.h"

#include "ast/StoreExtracted.h"
#include "ast/extractors/File.h"
#include "ast/extractors/NamedDecl.h"
#include "ast/extractors/RelationTarget.h"
#include "ast/visitors/SymbolCollector.h"
#include "storage/FactStore.h"
#include "storage/SemanticProperties.h"

#include <clang/AST/ASTContext.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/DeclTemplate.h>

namespace facts::commands::match {
namespace {
std::expected<clang::NamedDecl *, std::string>
supportedDeclaration(const clang::NamedDecl &node) {
  if (auto *value = llvm::dyn_cast<clang::ClassTemplateDecl>(&node))
    return const_cast<clang::CXXRecordDecl *>(value->getTemplatedDecl());
  if (auto *value = llvm::dyn_cast<clang::FunctionTemplateDecl>(&node))
    return const_cast<clang::FunctionDecl *>(value->getTemplatedDecl());
  if (auto *value = llvm::dyn_cast<clang::VarTemplateDecl>(&node))
    return const_cast<clang::VarDecl *>(value->getTemplatedDecl());
  if (llvm::isa<clang::TemplateDecl>(node))
    return std::unexpected("unsupported TemplateDecl binding");
  if (llvm::isa<clang::CXXRecordDecl, clang::FunctionDecl, clang::FieldDecl,
                clang::VarDecl, clang::EnumDecl, clang::EnumConstantDecl>(node))
    return const_cast<clang::NamedDecl *>(&node);
  return std::unexpected(std::string{"unsupported symbol declaration '"} +
                         node.getDeclKindName() + "'");
}

std::string kindName(const clang::NamedDecl &node) {
  if (llvm::isa<clang::TemplateDecl>(node))
    return "template";
  if (llvm::isa<clang::CXXRecordDecl>(node))
    return "record";
  if (llvm::isa<clang::CXXMethodDecl>(node))
    return "method";
  if (llvm::isa<clang::FunctionDecl>(node))
    return "function";
  if (llvm::isa<clang::FieldDecl>(node))
    return "field";
  if (llvm::isa<clang::EnumConstantDecl>(node))
    return "enumerator";
  if (llvm::isa<clang::EnumDecl>(node))
    return "enum";
  return "variable";
}

std::expected<MatchedSymbol, std::string>
indexRecord(const clang::NamedDecl &node, std::string usr,
            clang::ASTContext &context, FileManager &files) {
  return resolveFile(context.getSourceManager(), node.getLocation(), files)
      .transform_error([](auto) { return std::string{"unregistered-file"}; })
      .and_then([&](FileId file) -> std::expected<MatchedSymbol, std::string> {
        if (file == builtinFileId)
          return std::unexpected("unregistered-file");
        return MatchedSymbol{
            std::move(usr),
            extractQualifiedName(node, context.getSourceManager()), file,
            storage::storedSymbolKind(clang::index::getSymbolInfo(&node).Kind)};
      });
}
} // namespace

std::expected<PersistedSymbol, std::string>
persistSymbol(const clang::NamedDecl &bound, clang::ASTContext &context,
              FileManager &files, FactStore &store) {
  return supportedDeclaration(bound).and_then(
      [&](clang::NamedDecl *node)
          -> std::expected<PersistedSymbol, std::string> {
        auto usr = extractUsr(*node);
        if (!usr)
          return std::unexpected("cannot persist bound symbol '" +
                                 node->getQualifiedNameAsString() +
                                 "': invalid-usr");
        auto indexed = indexRecord(*node, *usr, context, files);
        auto collected = collectDeclaredSymbol(*node, context, files, store);
        if (!collected)
          return std::unexpected("cannot persist bound symbol '" +
                                 node->getQualifiedNameAsString() + "' usr='" +
                                 *usr + "': " + collected.error().message);
        auto id = store.findId(*usr);
        if (!id)
          return std::unexpected("cannot persist bound symbol '" +
                                 node->getQualifiedNameAsString() + "' usr='" +
                                 *usr + "': " + id.error().message());
        if (!*id) {
          auto external = resolveRelationTarget(
              *node, context.getSourceManager(), files, store);
          if (!external)
            return std::unexpected(
                "cannot persist bound symbol '" +
                node->getQualifiedNameAsString() + "' usr='" + *usr +
                "': " + std::string{extractionErrorName(external.error())});
          if (!*external)
            return std::unexpected("bound symbol '" +
                                   node->getQualifiedNameAsString() +
                                   "' usr='" + *usr + "' was not persisted");
          return PersistedSymbol{
              **external, kindName(bound),
              extractQualifiedName(*node, context.getSourceManager()),
              indexed ? std::optional{std::move(*indexed)} : std::nullopt,
              indexed ? "" : indexed.error()};
        }
        return PersistedSymbol{
            **id, kindName(bound),
            extractQualifiedName(*node, context.getSourceManager()),
            indexed ? std::optional{std::move(*indexed)} : std::nullopt,
            indexed ? "" : indexed.error()};
      });
}

} // namespace facts::commands::match
