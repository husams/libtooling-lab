#ifndef FACTS_TOOL_AST_EXTRACTORS_TYPE_H
#define FACTS_TOOL_AST_EXTRACTORS_TYPE_H

#include "model/SymbolId.h"

#include <expected>
#include <string>
#include <system_error>

namespace clang {
class QualType;
}

namespace facts {
class FactStore;

struct TypeResolutionError {
  std::string target;
  std::string usr;
  std::string detail;
  bool targetMissing = false;
};

using TypeResult = std::expected<SymbolId, TypeResolutionError>;

TypeResult extractType(const clang::QualType &type, FactStore &store);

} // namespace facts

#endif // FACTS_TOOL_AST_EXTRACTORS_TYPE_H
