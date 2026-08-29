#include "ast/extractors/Parameters.h"

#include "ast/extractors/ParmVarDecl.h"

#include "clang/AST/Decl.h"

#include <algorithm>
#include <ranges>
#include <utility>
#include <vector>

namespace facts {
namespace {

DetailedExtractionResult<std::vector<Parameter>>
appendParameter(std::vector<Parameter> parameters,
                const clang::ParmVarDecl &node,
                const clang::SourceManager &sourceManager, FileManager &files,
                FactStore &store) {
  auto append = [parameters = std::move(parameters)](
                    std::optional<Parameter> parameter) mutable
      -> DetailedExtractionResult<std::vector<Parameter>> {
    if (parameter) {
      parameters.push_back(std::move(*parameter));
    }
    return std::move(parameters);
  };

  return extractParameter(node, sourceManager, files, store) |
         std::move(append);
}

} // namespace

// FunctionDecl and its subclasses expose the source-ordered parameter
// sequence. Keeping list extraction here lets functions and methods reuse it.
DetailedExtractionResult<std::vector<Parameter>>
extractParameters(const clang::FunctionDecl &node,
                  const clang::SourceManager &sourceManager, FileManager &files,
                  FactStore &store) {
  const auto toExtractionStage = [&](const auto *parameter) {
    return [&, parameter](std::vector<Parameter> parameters) {
      return appendParameter(std::move(parameters), *parameter, sourceManager,
                             files, store);
    };
  };

  const auto applyStage = [](auto result, const auto &stage) {
    return std::move(result) | stage;
  };

  // Transform each AST parameter into one fallible append stage, then apply
  // the stages in source order while the expected pipeline propagates errors.
  const auto parameterStages =
      std::ranges::subrange(node.param_begin(), node.param_end()) |
      std::views::transform(toExtractionStage);

  return std::ranges::fold_left(
      parameterStages,
      DetailedExtractionResult<std::vector<Parameter>>{
          std::vector<Parameter>{}},
      applyStage);
}

} // namespace facts
