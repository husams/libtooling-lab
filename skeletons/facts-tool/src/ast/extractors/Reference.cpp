#include "ast/extractors/Reference.h"

#include "ast/extractors/File.h"
#include "ast/extractors/Location.h"
#include "ast/extractors/NamedDecl.h"
#include "ast/extractors/RelationTarget.h"
#include "cli/Trace.h"
#include "storage/FactStore.h"
#include "storage/FileManager.h"

#include <clang/AST/DeclTemplate.h>
#include <clang/AST/Expr.h>
#include <clang/AST/ExprCXX.h>
#include <llvm/Support/Casting.h>

#include <expected>
#include <optional>
#include <string>

namespace facts {
namespace {

ExtractionResult<std::optional<SymbolId>>
resolveStoredSymbol(const clang::NamedDecl &decl, FactStore &store) {
  return extractUsr(decl).and_then([&](std::string usr) {
    return store.findId(usr).transform_error(
        [](std::error_code) { return ExtractionError::InvalidUsr; });
  });
}

bool transparentCalleeWrapper(const clang::Expr &expression) {
  return llvm::isa<clang::ParenExpr, clang::ImplicitCastExpr,
                   clang::ExprWithCleanups>(expression);
}

const clang::Expr &unwrapCallee(const clang::Expr &expression) {
  const clang::Expr *current = &expression;
  while (transparentCalleeWrapper(*current)) {
    if (const auto *paren = llvm::dyn_cast<clang::ParenExpr>(current)) {
      current = paren->getSubExpr();
    } else if (const auto *cast =
                   llvm::dyn_cast<clang::ImplicitCastExpr>(current)) {
      current = cast->getSubExpr();
    } else {
      current = llvm::cast<clang::ExprWithCleanups>(current)->getSubExpr();
    }
  }
  return *current;
}

ExtractionResult<std::optional<UseFact>>
addTarget(const clang::NamedDecl &referenced, clang::SourceLocation site,
          const clang::SourceManager &sourceManager, FileManager &files,
          FactStore &store, SymbolId source) {
  if (const auto *variable = llvm::dyn_cast<clang::VarDecl>(&referenced);
      variable && variable->isLocalVarDeclOrParm()) {
    cli::logVerbose(store.verbosity(), 3,
                    "facts-tool: trace: reference target name='{}' "
                    "result=filtered reason='local declaration outside symbol "
                    "model'",
                    referenced.getQualifiedNameAsString());
    return std::nullopt;
  }
  return resolveRelationTarget(referenced, sourceManager, files, store)
      .and_then([&](std::optional<SymbolId> destination)
                    -> ExtractionResult<std::optional<UseFact>> {
        if (!destination) {
          return std::nullopt;
        }
        const auto location = extractLocation(sourceManager, site);
        if (!location) {
          return std::nullopt;
        }
        const auto file = resolveFile(sourceManager, site, files);
        if (!file) {
          return std::nullopt;
        }
        const Relation relation{
            .source = source,
            .destination = *destination,
            .kind = RelationKind::Uses,
        };
        return UseFact{
            .relation = relation,
            .site =
                RelationSite{
                    .source = source,
                    .destination = *destination,
                    .kind = RelationKind::Uses,
                    .file = *file,
                    .location = *location,
                },
        };
      });
}

} // namespace

void ReferenceContext::enter(const clang::CallExpr &call) {
  const auto &callee = unwrapCallee(*call.getCallee());
  directCallees_.push_back(&callee);
}

void ReferenceContext::leave() { directCallees_.pop_back(); }

bool ReferenceContext::isDirectCallee(const clang::Expr &expression) const {
  return !directCallees_.empty() && directCallees_.back() == &expression;
}

ReferenceDisposition classifyReference(const clang::Expr &expression,
                                       const ReferenceContext &context) {
  if (expression.isValueDependent()) {
    return ReferenceDisposition::Skip;
  }
  return context.isDirectCallee(expression)
             ? ReferenceDisposition::SpecificRelation
             : ReferenceDisposition::Uses;
}

const clang::FunctionDecl &referenceOwner(const clang::FunctionDecl &decl) {
  const auto specialization = decl.getTemplateSpecializationKind();
  if (specialization != clang::TSK_ExplicitSpecialization) {
    if (const auto *pattern = decl.getTemplateInstantiationPattern()) {
      return *pattern;
    }
  }
  return decl;
}

ExtractionResult<std::optional<UseFact>> extractUseReference(
    const clang::FunctionDecl &owner, const clang::NamedDecl &referenced,
    clang::SourceLocation site, const clang::SourceManager &sourceManager,
    FileManager &files, FactStore &store) {
  return resolveStoredSymbol(owner, store)
      .and_then([&](std::optional<SymbolId> source)
                    -> ExtractionResult<std::optional<UseFact>> {
        return source ? addTarget(referenced, site, sourceManager, files, store,
                                  *source)
                      : ExtractionResult<std::optional<UseFact>>{std::nullopt};
      });
}

} // namespace facts
