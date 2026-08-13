#ifndef FACTS_TOOL_AST_EXTRACTORS_EXTRACTION_H
#define FACTS_TOOL_AST_EXTRACTORS_EXTRACTION_H

#include <expected>
#include <utility>

namespace facts {

enum class ExtractionError {
  InvalidSourceLocation,
  InvalidSourceRange,
  OutsideMainFile,
  SystemHeader,
  InvalidPresumedLocation,
  InvalidUsr,
};

template <typename ValueT>
using ExtractionResult = std::expected<ValueT, ExtractionError>;

template <typename ValueT, typename ErrorT, typename NextT>
auto operator|(std::expected<ValueT, ErrorT> result, NextT &&next)
    -> decltype(std::move(result).and_then(std::forward<NextT>(next))) {
  return std::move(result).and_then(std::forward<NextT>(next));
}

} // namespace facts

#endif // FACTS_TOOL_AST_EXTRACTORS_EXTRACTION_H
