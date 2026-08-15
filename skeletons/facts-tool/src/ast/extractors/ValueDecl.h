#ifndef FACTS_TOOL_AST_EXTRACTORS_VALUEDECL_H
#define FACTS_TOOL_AST_EXTRACTORS_VALUEDECL_H

#include "model/SymbolId.h"

#include <expected>
#include <system_error>

namespace clang {
class ValueDecl;
} // namespace clang

namespace facts {
class FactStore;

std::expected<void, std::error_code>
storeValueRelations(const clang::ValueDecl &node, SymbolId value,
                    FactStore &store);

} // namespace facts

#endif // FACTS_TOOL_AST_EXTRACTORS_VALUEDECL_H
