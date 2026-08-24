#include "ast/visitors/SymbolVisitor.h"

#include "ast/extractors/Reference.h"
#include "ast/visitors/BodyVisitor.h"
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
  if (decl == nullptr || !VisitNamedDecl(decl)) {
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

bool SymbolVisitor::TraverseStmt(clang::Stmt *statement) {
  return statement == nullptr || bodyOwners_.contains(statement) ||
         Base::TraverseStmt(statement);
}

bool SymbolVisitor::TraverseUsingDirectiveDecl(clang::UsingDirectiveDecl *) {
  return true;
}

bool SymbolVisitor::VisitFunctionDecl(clang::FunctionDecl *decl) {
  schedule(*decl);
  return true;
}

void SymbolVisitor::schedule(const clang::FunctionDecl &decl) {
  const auto &owner = referenceOwner(decl);
  if (!owner.doesThisDeclarationHaveABody()) {
    return;
  }
  auto *body = owner.getBody();
  if (body != nullptr && bodyOwners_.try_emplace(body, &owner).second) {
    pendingBodies_.emplace_back(&owner, body);
  }
}

IndexingResult SymbolVisitor::flushBodies() {
  for (const auto &[owner, body] : pendingBodies_) {
    if (!visitedBodies_.insert(body).second) {
      continue;
    }
    BodyVisitor visitor(*owner, context_, files_, store_, status_);
    if (!visitor.TraverseStmt(body)) {
      return std::unexpected(IndexingError{"cannot traverse function body"});
    }
    auto flushed = visitor.flush();
    if (!flushed) {
      return flushed;
    }
  }
  return {};
}

bool SymbolVisitor::VisitNamedDecl(clang::NamedDecl *decl) {
  status_.record(collectDeclaredSymbol(*decl, context_, files_, store_));
  return true;
}

} // namespace facts
