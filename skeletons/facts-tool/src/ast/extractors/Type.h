#ifndef FACTS_TOOL_AST_EXTRACTORS_TYPE_H
#define FACTS_TOOL_AST_EXTRACTORS_TYPE_H

#include "model/SymbolId.h"

#include <expected>
#include <system_error>

namespace clang {
class QualType;
}

namespace facts {
class FactStore;

std::expected<SymbolId, std::error_code>
extractType(const clang::QualType &type, FactStore &store);

} // namespace facts

#endif // FACTS_TOOL_AST_EXTRACTORS_TYPE_H
