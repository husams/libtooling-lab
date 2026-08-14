#include "ast/visitors/SymbolVisitor.h"

#include "ast/visitors/SymbolCollector.h"

namespace facts {

bool SymbolVisitor::TraverseCXXMethodDecl(clang::CXXMethodDecl *decl) {
  return decl == nullptr || WalkUpFromCXXMethodDecl(decl);
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
