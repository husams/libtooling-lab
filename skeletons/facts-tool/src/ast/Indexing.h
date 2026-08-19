#ifndef FACTS_TOOL_AST_INDEXING_H
#define FACTS_TOOL_AST_INDEXING_H

#include <llvm/Support/raw_ostream.h>

#include <cstddef>
#include <expected>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace facts {

struct IndexingError {
  std::string message;
};

using IndexingResult = std::expected<void, IndexingError>;

inline IndexingError contextualize(std::string_view context,
                                   std::error_code error) {
  return IndexingError{std::string{context} + ": " + error.message()};
}

inline IndexingError contextualize(std::string_view, IndexingError error) {
  return error;
}

template <typename Value, typename Error>
std::expected<Value, IndexingError>
withContext(std::expected<Value, Error> result, std::string_view context) {
  return std::move(result).transform_error(
      [context](Error error) { return contextualize(context, error); });
}

class IndexingStatus {
public:
  void record(IndexingResult result) {
    if (result) {
      return;
    }

    ++failureCount_;
    llvm::errs() << "facts-tool: indexing incomplete: "
                 << result.error().message << '\n';
  }

  [[nodiscard]] bool complete() const { return failureCount_ == 0; }

  [[nodiscard]] std::size_t failureCount() const { return failureCount_; }

private:
  std::size_t failureCount_ = 0;
};

} // namespace facts

#endif // FACTS_TOOL_AST_INDEXING_H
