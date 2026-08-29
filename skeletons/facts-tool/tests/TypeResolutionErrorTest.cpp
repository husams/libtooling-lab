#include "ast/StoreExtracted.h"
#include "ast/extractors/EnumDecl.h"
#include "ast/extractors/ParmVarDecl.h"
#include "ast/extractors/TemplatePattern.h"
#include "ast/extractors/TemplateSpecialization.h"
#include "ast/extractors/Type.h"

#include <cassert>
#include <concepts>
#include <expected>
#include <string>
#include <utility>

namespace {

template <typename Result>
concept DetailedResult =
    std::same_as<typename Result::error_type, facts::DetailedExtractionError>;

static_assert(DetailedResult<decltype(facts::extractParameter(
                  std::declval<const clang::ParmVarDecl &>(),
                  std::declval<const clang::SourceManager &>(),
                  std::declval<facts::FileManager &>(),
                  std::declval<facts::FactStore &>()))>);

static_assert(DetailedResult<decltype(facts::extractEnumeration(
                  std::declval<const clang::EnumDecl &>(),
                  std::declval<const clang::SourceManager &>(),
                  std::declval<facts::FileManager &>(),
                  std::declval<facts::FactStore &>()))>);

static_assert(DetailedResult<decltype(facts::extractTemplateArguments(
                  std::declval<const clang::TemplateParameterList &>(),
                  std::declval<const clang::SourceManager &>(),
                  std::declval<facts::FileManager &>(),
                  std::declval<facts::FactStore &>()))>);

static_assert(DetailedResult<decltype(facts::extractTemplateParameters(
                  std::declval<llvm::ArrayRef<clang::TemplateArgument>>(),
                  std::declval<const clang::ASTContext &>(),
                  std::declval<facts::FileManager &>(),
                  std::declval<facts::FactStore &>()))>);

} // namespace

int main() {
  facts::TypeResult failed = std::unexpected(facts::TypeResolutionError{
      .target = "review::ExternalType",
      .usr = "c:@N@review@S@ExternalType",
      .detail = "forced persistence failure",
  });

  const auto detailed =
      std::move(failed).transform_error(facts::typeExtractionFailure);
  assert(!detailed);
  const auto diagnostic = facts::extractionErrorName(detailed.error());
  assert(diagnostic.find("review::ExternalType") != std::string::npos);
  assert(diagnostic.find("c:@N@review@S@ExternalType") != std::string::npos);
  assert(diagnostic.find("forced persistence failure") != std::string::npos);
}
