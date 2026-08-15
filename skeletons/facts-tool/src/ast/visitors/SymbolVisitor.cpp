#include "ast/visitors/SymbolVisitor.h"

#include "ast/extractors/EnumConstantDecl.h"
#include "ast/extractors/EnumDecl.h"
#include "ast/extractors/FieldDecl.h"
#include "ast/extractors/VarDecl.h"
#include "ast/visitors/SymbolCollector.h"

namespace facts {

bool SymbolVisitor::TraverseCXXMethodDecl(clang::CXXMethodDecl *decl) {
  return decl == nullptr || WalkUpFromCXXMethodDecl(decl);
}

bool SymbolVisitor::TraverseFieldDecl(clang::FieldDecl *decl) {
  if (decl == nullptr || !VisitFieldDecl(decl)) {
    return decl == nullptr;
  }

  if (auto *bitWidth = decl->getBitWidth();
      bitWidth != nullptr && !TraverseStmt(bitWidth)) {
    return false;
  }

  auto *initializer = decl->getInClassInitializer();
  return initializer == nullptr || TraverseStmt(initializer);
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

bool SymbolVisitor::VisitEnumDecl(clang::EnumDecl *decl) {
  collectSymbol(*decl, context_, files_, store_);
  return true;
}

bool SymbolVisitor::VisitEnumConstantDecl(clang::EnumConstantDecl *decl) {
  collectSymbol(*decl, context_, files_, store_);
  return true;
}

bool SymbolVisitor::VisitFieldDecl(clang::FieldDecl *decl) {
  collectSymbol(*decl, context_, files_, store_);
  return true;
}

bool SymbolVisitor::VisitVarDecl(clang::VarDecl *decl) {
  collectSymbol(*decl, context_, files_, store_);
  return true;
}

bool SymbolVisitor::VisitNamedDecl(clang::NamedDecl *decl) {
  collectSymbol(*decl, context_, files_, store_);
  return true;
}

} // namespace facts
