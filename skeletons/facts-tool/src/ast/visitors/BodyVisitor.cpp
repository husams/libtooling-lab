#include "ast/visitors/BodyVisitor.h"

#include "ast/StoreExtracted.h"
#include "ast/visitors/SymbolCollector.h"
#include "storage/FactStore.h"

#include <clang/AST/ASTContext.h>
#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/Expr.h>
#include <clang/AST/ExprCXX.h>

#include <algorithm>
#include <ranges>
#include <string>
#include <tuple>

namespace facts {
namespace {

auto relationKey(const Relation &relation) {
  return std::tuple{relation.source, relation.destination, relation.kind,
                    relation.position};
}

std::vector<Relation> canonicalRelations(const std::vector<UseFact> &facts) {
  auto relations = facts | std::views::transform(&UseFact::relation) |
                   std::ranges::to<std::vector>();
  std::ranges::sort(relations, {}, relationKey);
  relations.erase(std::ranges::unique(relations, {}, relationKey).begin(),
                  relations.end());
  return relations;
}

std::vector<RelationSite> relationSites(const std::vector<UseFact> &facts) {
  return facts | std::views::transform(&UseFact::site) |
         std::ranges::to<std::vector>();
}

} // namespace

void BodyVisitor::capture(ExtractionResult<std::optional<UseFact>> fact,
                          const clang::NamedDecl &referenced) {
  if (!fact) {
    // An unnamed declaration -- an anonymous union/struct member, an unnamed
    // parameter -- has no stable USR to key a relation on. Skipping it is
    // correct; escalating it aborts the whole translation unit.
    if (fact.error() == ExtractionError::InvalidUsr &&
        !referenced.getDeclName()) {
      return;
    }
    status_.record(std::unexpected(
        IndexingError{"cannot extract reference to '" +
                      referenced.getQualifiedNameAsString() +
                      "': " + std::string{extractionErrorName(fact.error())}}));
    return;
  }
  if (*fact) {
    facts_.push_back(std::move(**fact));
  }
}

void BodyVisitor::capture(const clang::Expr &expression,
                          const clang::NamedDecl &referenced,
                          clang::SourceLocation location) {
  if (classifyReference(expression, context_) != ReferenceDisposition::Uses) {
    return;
  }
  capture(extractUseReference(owner_, referenced, location,
                              context_.getSourceManager(), files_, store_),
          referenced);
}

void BodyVisitor::schedule(const clang::FunctionDecl &decl) {
  const auto &owner = referenceOwner(decl);
  if (!owner.doesThisDeclarationHaveABody()) {
    return;
  }
  auto *body = owner.getBody();
  if (body != nullptr && scheduledBodies_.insert(body).second) {
    pendingBodies_.emplace_back(&owner, body);
  }
}

bool BodyVisitor::TraverseFunctionDecl(clang::FunctionDecl *decl) {
  if (decl == nullptr) {
    return true;
  }
  status_.record(collectDeclaredSymbol(*decl, context_, files_, store_));
  schedule(*decl);
  return true;
}

bool BodyVisitor::TraverseCXXMethodDecl(clang::CXXMethodDecl *decl) {
  return TraverseFunctionDecl(decl);
}

bool BodyVisitor::TraverseLambdaExpr(clang::LambdaExpr *expression) {
  if (expression == nullptr) {
    return true;
  }
  const auto capturesTraversed = std::ranges::all_of(
      expression->capture_inits(),
      [&](clang::Expr *capture) { return TraverseStmt(capture); });
  if (!capturesTraversed) {
    return false;
  }

  auto *closure = expression->getLambdaClass();
  status_.record(collectDeclaredSymbol(*closure, context_, files_, store_));
  auto *callOperator = expression->getCallOperator();
  status_.record(
      collectDeclaredSymbol(*callOperator, context_, files_, store_));
  schedule(*callOperator);
  return true;
}

bool BodyVisitor::VisitDeclRefExpr(clang::DeclRefExpr *expression) {
  capture(*expression, *expression->getDecl(), expression->getExprLoc());
  return true;
}

bool BodyVisitor::VisitMemberExpr(clang::MemberExpr *expression) {
  capture(*expression, *expression->getMemberDecl(),
          expression->getMemberLoc());
  return true;
}

bool BodyVisitor::VisitNamedDecl(clang::NamedDecl *decl) {
  status_.record(collectDeclaredSymbol(*decl, context_, files_, store_));
  return true;
}

IndexingResult BodyVisitor::flushNestedBodies() {
  for (const auto &[owner, body] : pendingBodies_) {
    BodyVisitor visitor(*owner, context_, files_, store_, status_);
    if (!visitor.TraverseStmt(body)) {
      return std::unexpected(
          IndexingError{"cannot traverse nested function body"});
    }
    auto flushed = visitor.flush();
    if (!flushed) {
      return flushed;
    }
  }
  return {};
}

IndexingResult BodyVisitor::persistUses() {
  if (facts_.empty()) {
    return {};
  }
  auto relations = canonicalRelations(facts_);
  auto sites = relationSites(facts_);
  return withContext(store_.addUseFacts(relations, sites),
                     "cannot persist function-body use facts");
}

IndexingResult BodyVisitor::flush() {
  return flushNestedBodies().and_then([&] { return persistUses(); });
}

} // namespace facts
