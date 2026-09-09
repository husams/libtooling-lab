#ifndef FACTS_TOOL_AST_EXTRACTORS_REFERENCE_H
#define FACTS_TOOL_AST_EXTRACTORS_REFERENCE_H

#include "ast/extractors/Extraction.h"
#include "model/Relation.h"
#include "model/RelationSite.h"

#include <optional>
#include <vector>

namespace clang {
class CallExpr;
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

class ReferenceContext final {
public:
  void enter(const clang::CallExpr &call);
  void leave();
  [[nodiscard]] bool isDirectCallee(const clang::Expr &expression) const;

private:
  std::vector<const clang::Expr *> directCallees_;
};

struct UseFact {
  Relation relation;
  RelationSite site;
};

ReferenceDisposition classifyReference(const clang::Expr &expression,
                                       const ReferenceContext &context);

const clang::FunctionDecl &referenceOwner(const clang::FunctionDecl &decl);

ExtractionResult<std::optional<UseFact>> extractUseReference(
    const clang::FunctionDecl &owner, const clang::NamedDecl &referenced,
    clang::SourceLocation site, const clang::SourceManager &sourceManager,
    FileManager &files, FactStore &store);

} // namespace facts

#endif // FACTS_TOOL_AST_EXTRACTORS_REFERENCE_H
