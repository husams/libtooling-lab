#ifndef FACTS_TOOL_AST_EXTRACTORS_TYPE_H
#define FACTS_TOOL_AST_EXTRACTORS_TYPE_H

#include "model/SymbolId.h"

#include <expected>
#include <string>
#include <system_error>

namespace clang {
class QualType;
class SourceManager;
} // namespace clang

namespace facts {
class FactStore;
class FileManager;

struct TypeResolutionError {
  std::string target;
  std::string usr;
  std::string detail;
};

using TypeResult = std::expected<SymbolId, TypeResolutionError>;

TypeResult extractType(const clang::QualType &type,
                       const clang::SourceManager &sourceManager,
                       FileManager &files, FactStore &store);

} // namespace facts

#endif // FACTS_TOOL_AST_EXTRACTORS_TYPE_H
