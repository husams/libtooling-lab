#ifndef FACTS_TOOL_AST_INDEXING_H
#define FACTS_TOOL_AST_INDEXING_H

#include <llvm/Support/raw_ostream.h>

#include <cstddef>
#include <expected>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_set>
#include <utility>

namespace facts {

struct IndexingError {
  std::string message;
  std::string category;
};

using IndexingResult = std::expected<void, IndexingError>;

inline IndexingError contextualize(std::string_view context,
                                   std::error_code error) {
  return IndexingError{std::string{context} + ": " + error.message()};
}

inline IndexingError contextualize(std::string_view, IndexingError error) {
  return error;
}

inline IndexingError
relationFailure(std::string_view relation, std::string_view sourceLabel,
                std::string_view source, std::string_view targetLabel,
                std::string_view target, std::string_view usr,
                std::string_view detail) {
  return IndexingError{"cannot persist relation=" + std::string{relation} +
                       " " + std::string{sourceLabel} + "='" +
                       std::string{source} + "' " + std::string{targetLabel} +
                       "='" + std::string{target} + "' usr='" +
                       std::string{usr} + "': " + std::string{detail}};
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
    const auto &error = result.error();
    const auto &key = error.category.empty() ? error.message : error.category;
    if (reported_.insert(key).second) {
      llvm::errs() << "facts-tool: indexing incomplete: " << error.message
                   << '\n';
    }
  }

  [[nodiscard]] bool complete() const { return failureCount_ == 0; }

  [[nodiscard]] std::size_t failureCount() const { return failureCount_; }

private:
  std::size_t failureCount_ = 0;
  std::unordered_set<std::string> reported_;
};

} // namespace facts

#endif // FACTS_TOOL_AST_INDEXING_H
