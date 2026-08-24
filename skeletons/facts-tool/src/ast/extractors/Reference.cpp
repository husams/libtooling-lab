#include "ast/extractors/Reference.h"

#include "ast/extractors/File.h"
#include "ast/extractors/Location.h"
#include "ast/extractors/NamedDecl.h"
#include "storage/FactStore.h"
#include "storage/FileManager.h"

#include <clang/AST/ASTContext.h>
#include <clang/AST/DeclTemplate.h>
#include <clang/AST/Expr.h>
#include <clang/AST/ExprCXX.h>
#include <clang/AST/ParentMapContext.h>
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

bool isDirectCallee(const clang::Expr &expression, clang::ASTContext &context) {
  const clang::Expr *current = &expression;
  while (true) {
    const auto parents = context.getParents(*current);
    if (parents.size() != 1) {
      return false;
    }
    if (const auto *call = parents[0].get<clang::CallExpr>()) {
      return call->getCallee()->IgnoreParenImpCasts() ==
             expression.IgnoreParenImpCasts();
    }
    const auto *parent = parents[0].get<clang::Expr>();
    if (parent == nullptr || !transparentCalleeWrapper(*parent)) {
      return false;
    }
    current = parent;
  }
}

ExtractionResult<std::optional<UseFact>>
addTarget(const clang::NamedDecl &referenced, clang::SourceLocation site,
          const clang::SourceManager &sourceManager, FileManager &files,
          FactStore &store, SymbolId source) {
  return resolveStoredSymbol(referenced, store)
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

ReferenceDisposition classifyReference(const clang::Expr &expression,
                                       clang::ASTContext &context) {
  if (expression.isValueDependent()) {
    return ReferenceDisposition::Skip;
  }
  return isDirectCallee(expression, context)
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
