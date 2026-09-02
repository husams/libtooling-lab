#include "commands/match/SymbolDispatch.h"

#include "ast/extractors/NamedDecl.h"
#include "ast/visitors/SymbolCollector.h"
#include "storage/FactStore.h"

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
} // namespace

std::expected<PersistedSymbol, std::string>
persistSymbol(const clang::NamedDecl &bound, clang::ASTContext &context,
              FileManager &files, FactStore &store) {
  return supportedDeclaration(bound).and_then(
      [&](clang::NamedDecl *node)
          -> std::expected<PersistedSymbol, std::string> {
        auto usr = extractUsr(*node);
        if (!usr)
          return std::unexpected("cannot extract bound symbol USR");
        auto collected = collectDeclaredSymbol(*node, context, files, store);
        if (!collected)
          return std::unexpected(collected.error().message);
        auto id = store.findId(*usr);
        if (!id)
          return std::unexpected("cannot look up persisted symbol: " +
                                 id.error().message());
        if (!*id)
          return std::unexpected("bound symbol was not persisted");
        return PersistedSymbol{
            **id, kindName(bound),
            extractQualifiedName(*node, context.getSourceManager())};
      });
}

} // namespace facts::commands::match
