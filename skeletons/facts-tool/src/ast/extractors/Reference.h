#ifndef FACTS_TOOL_AST_EXTRACTORS_REFERENCE_H
#define FACTS_TOOL_AST_EXTRACTORS_REFERENCE_H

#include "ast/extractors/Extraction.h"
#include "model/Relation.h"
#include "model/RelationSite.h"

#include <optional>

namespace clang {
class ASTContext;
class Expr;
class FunctionDecl;
class NamedDecl;
class SourceLocation;
class SourceManager;
} // namespace clang

namespace facts {
class FactStore;
class FileManager;

enum class ReferenceDisposition { Uses, SpecificRelation, Skip };

struct UseFact {
  Relation relation;
  RelationSite site;
};

ReferenceDisposition classifyReference(const clang::Expr &expression,
                                       clang::ASTContext &context);

const clang::FunctionDecl &referenceOwner(const clang::FunctionDecl &decl);

ExtractionResult<std::optional<UseFact>> extractUseReference(
    const clang::FunctionDecl &owner, const clang::NamedDecl &referenced,
    clang::SourceLocation site, const clang::SourceManager &sourceManager,
    FileManager &files, FactStore &store);

} // namespace facts

#endif // FACTS_TOOL_AST_EXTRACTORS_REFERENCE_H
