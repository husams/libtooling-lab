#ifndef FACTS_TOOL_AST_EXTRACTORS_INITIALIZER_H
#define FACTS_TOOL_AST_EXTRACTORS_INITIALIZER_H

#include "model/Initializer.h"

#include <optional>

namespace clang {
class ASTContext;
class Expr;
class QualType;
class SourceManager;
} // namespace clang

namespace facts {

std::optional<Initializer>
extractInitializer(const clang::Expr *expression,
                   const clang::QualType &declaredType,
                   const clang::ASTContext &context,
                   const clang::SourceManager &sourceManager);

} // namespace facts

#endif // FACTS_TOOL_AST_EXTRACTORS_INITIALIZER_H
