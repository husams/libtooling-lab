#include "ast/extractors/TemplatePattern.h"

#include "ast/extractors/Type.h"
#include "storage/FactStore.h"

#include <clang/AST/DeclTemplate.h>
#include <llvm/Support/Casting.h>

#include <ranges>
#include <utility>

namespace facts {
namespace {

std::uint32_t flagWhen(TemplateArgumentBit flag, bool condition) {
  return condition ? bit(static_cast<std::size_t>(flag)) : 0;
}

DetailedExtractionResult<TemplateArgument>
extractTemplateArgument(const clang::NamedDecl &node,
                        const clang::SourceManager &sourceManager,
                        FileManager &files, FactStore &store) {
  if (const auto *parameter =
          llvm::dyn_cast<clang::TemplateTypeParmDecl>(&node)) {
    return TemplateArgument{
        .name = parameter->getNameAsString(),
        .flags = flagWhen(ParameterPackBit, parameter->isParameterPack()),
    };
  }

  if (const auto *parameter =
          llvm::dyn_cast<clang::NonTypeTemplateParmDecl>(&node)) {
    return extractType(parameter->getType(), sourceManager, files, store)
        .transform_error(typeExtractionFailure)
        .transform([&](std::optional<SymbolId> type) {
          return TemplateArgument{
              .name = parameter->getNameAsString(),
              .type = type.value_or(SymbolId{}),
              .flags =
                  flagWhen(ParameterPackBit, parameter->isParameterPack()) |
                  bit(static_cast<std::size_t>(NonTypeBit)),
          };
        });
  }

  if (const auto *parameter =
          llvm::dyn_cast<clang::TemplateTemplateParmDecl>(&node)) {
    return TemplateArgument{
        .name = parameter->getNameAsString(),
        .flags = flagWhen(ParameterPackBit, parameter->isParameterPack()) |
                 bit(static_cast<std::size_t>(TemplateTemplateBit)),
    };
  }

  return std::unexpected(DetailedExtractionError{ExtractionError::InvalidType});
}

} // namespace

DetailedExtractionResult<std::vector<TemplateArgument>>
extractTemplateArguments(const clang::TemplateParameterList &parameters,
                         const clang::SourceManager &sourceManager,
                         FileManager &files, FactStore &store) {
  const auto append =
      [&](DetailedExtractionResult<std::vector<TemplateArgument>> result,
          const clang::NamedDecl *parameter) {
        return std::move(result).and_then(
            [&](std::vector<TemplateArgument> arguments) {
              return extractTemplateArgument(*parameter, sourceManager, files,
                                             store)
                  .transform([arguments = std::move(arguments)](
                                 TemplateArgument argument) mutable {
                    arguments.push_back(std::move(argument));
                    return std::move(arguments);
                  });
            });
      };

  return std::ranges::fold_left(
      parameters,
      DetailedExtractionResult<std::vector<TemplateArgument>>{
          std::vector<TemplateArgument>{}},
      append);
}

} // namespace facts
