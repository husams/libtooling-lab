#include "ast/visitors/SymbolVisitor.h"

#include "ast/extractors/EnumConstantDecl.h"
#include "ast/extractors/EnumDecl.h"
#include "ast/extractors/FieldDecl.h"
#include "ast/extractors/VarDecl.h"
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

bool SymbolVisitor::TraverseUsingDirectiveDecl(clang::UsingDirectiveDecl *) {
  return true;
}

bool SymbolVisitor::VisitFunctionDecl(clang::FunctionDecl *decl) {
  status_.record(collectSymbol(*decl, context_, files_, store_));
  return true;
}

bool SymbolVisitor::VisitCXXRecordDecl(clang::CXXRecordDecl *decl) {
  status_.record(collectSymbol(*decl, context_, files_, store_,
                               decl->isThisDeclarationADefinition()));
  return true;
}

bool SymbolVisitor::VisitEnumDecl(clang::EnumDecl *decl) {
  status_.record(collectSymbol(*decl, context_, files_, store_));
  return true;
}

bool SymbolVisitor::VisitEnumConstantDecl(clang::EnumConstantDecl *decl) {
  status_.record(collectSymbol(*decl, context_, files_, store_));
  return true;
}

bool SymbolVisitor::VisitFieldDecl(clang::FieldDecl *decl) {
  status_.record(collectSymbol(*decl, context_, files_, store_));
  return true;
}

bool SymbolVisitor::VisitVarDecl(clang::VarDecl *decl) {
  status_.record(collectSymbol(*decl, context_, files_, store_));
  return true;
}

bool SymbolVisitor::VisitNamedDecl(clang::NamedDecl *decl) {
  status_.record(collectSymbol(*decl, context_, files_, store_));
  return true;
}

} // namespace facts
