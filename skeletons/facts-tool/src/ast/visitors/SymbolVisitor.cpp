#include "ast/visitors/SymbolVisitor.h"

#include "ast/visitors/SymbolCollector.h"

#include <algorithm>
#include <clang/Basic/SourceManager.h>

namespace facts {
namespace {

bool isSystemHeaderDeclaration(const clang::Decl &decl,
                               const clang::SourceManager &sourceManager) {
  const auto location = decl.getLocation();
  return location.isValid() && sourceManager.isInSystemHeader(location);
}

} // namespace

bool SymbolVisitor::TraverseTranslationUnitDecl(
    clang::TranslationUnitDecl *decl) {
  const auto &sourceManager = context_.getSourceManager();
  return std::ranges::all_of(decl->decls(), [&](clang::Decl *child) {
    return isSystemHeaderDeclaration(*child, sourceManager) ||
           TraverseDecl(child);
  });
}

bool SymbolVisitor::TraverseParmVarDecl(clang::ParmVarDecl *) { return true; }

bool SymbolVisitor::VisitFunctionDecl(clang::FunctionDecl *decl) {
  collectSymbol(*decl, context_, files_, store_);
  return true;
}

bool SymbolVisitor::VisitCXXRecordDecl(clang::CXXRecordDecl *decl) {
  collectSymbol(*decl, context_, files_, store_,
                decl->isThisDeclarationADefinition());
  return true;
}

bool SymbolVisitor::VisitNamedDecl(clang::NamedDecl *decl) {
  collectSymbol(*decl, context_, files_, store_);
  return true;
}

} // namespace facts
