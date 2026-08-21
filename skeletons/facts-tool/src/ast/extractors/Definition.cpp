#include "ast/extractors/Definition.h"

#include "ast/extractors/Location.h"

#include <clang/AST/ASTContext.h>
#include <clang/AST/DeclBase.h>

#include <utility>

namespace facts {

template <typename Model>
ExtractionResult<Model>
addDefinitionRegion(Model model, const clang::Decl &node, bool isDefinition,
                    const clang::SourceManager &sourceManager) {
  if (!isDefinition) {
    return model;
  }

  auto toDefinedModel =
      [model = std::move(model)](
          Region definition) mutable -> ExtractionResult<Model> {
    model.definition = definition;
    model.flags |= bit(DefinitionBit);
    return std::move(model);
  };

  return extractRegion(sourceManager, node.getASTContext().getLangOpts(),
                       node.getSourceRange()) |
         std::move(toDefinedModel);
}

template ExtractionResult<Function>
addDefinitionRegion(Function model, const clang::Decl &node, bool isDefinition,
                    const clang::SourceManager &sourceManager);

template ExtractionResult<Enumeration>
addDefinitionRegion(Enumeration model, const clang::Decl &node,
                    bool isDefinition,
                    const clang::SourceManager &sourceManager);

template ExtractionResult<Enumerator>
addDefinitionRegion(Enumerator model, const clang::Decl &node,
                    bool isDefinition,
                    const clang::SourceManager &sourceManager);

template ExtractionResult<Record>
addDefinitionRegion(Record model, const clang::Decl &node, bool isDefinition,
                    const clang::SourceManager &sourceManager);

template ExtractionResult<Variable>
addDefinitionRegion(Variable model, const clang::Decl &node, bool isDefinition,
                    const clang::SourceManager &sourceManager);

} // namespace facts
