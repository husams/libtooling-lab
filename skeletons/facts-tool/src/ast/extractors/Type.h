#ifndef FACTS_TOOL_AST_EXTRACTORS_TYPE_H
#define FACTS_TOOL_AST_EXTRACTORS_TYPE_H

#include "ast/extractors/Extraction.h"
#include "model/SymbolId.h"

#include <expected>
#include <optional>
#include <string>
#include <system_error>
#include <variant>

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

using TypeResult = std::expected<std::optional<SymbolId>, TypeResolutionError>;

using DetailedExtractionError =
    std::variant<ExtractionError, TypeResolutionError>;

template <typename ValueT>
using DetailedExtractionResult = std::expected<ValueT, DetailedExtractionError>;

inline DetailedExtractionError
typeExtractionFailure(TypeResolutionError error) {
  return error;
}

TypeResult extractType(const clang::QualType &type,
                       const clang::SourceManager &sourceManager,
                       FileManager &files, FactStore &store);

} // namespace facts

#endif // FACTS_TOOL_AST_EXTRACTORS_TYPE_H
