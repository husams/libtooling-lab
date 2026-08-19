#include "ast/extractors/RecordInstance.h"

#include "ast/extractors/RecordDecl.h"
#include "ast/extractors/TemplatePattern.h"
#include "ast/extractors/TemplateSpecialization.h"

#include <clang/AST/DeclTemplate.h>
#include <llvm/Support/Casting.h>

#include <utility>
#include <vector>

namespace facts {
namespace {

ExtractionResult<std::vector<TemplateArgument>>
extractOpenArguments(const clang::ClassTemplateSpecializationDecl &node,
                     FactStore &store) {
  const auto *partial =
      llvm::dyn_cast<clang::ClassTemplatePartialSpecializationDecl>(&node);
  return partial == nullptr
             ? ExtractionResult<std::vector<TemplateArgument>>{std::vector<
                   TemplateArgument>{}}
             : extractTemplateArguments(*partial->getTemplateParameters(),
                                        store);
}

const clang::NamedDecl *
specializedPattern(const clang::ClassTemplateSpecializationDecl &node) {
  const auto specialized = node.getSpecializedTemplateOrPartial();
  if (const auto *primary =
          specialized.dyn_cast<clang::ClassTemplateDecl *>()) {
    return primary->getTemplatedDecl();
  }
  return llvm::cast<clang::ClassTemplatePartialSpecializationDecl *>(
      specialized);
}

} // namespace

ExtractionResult<RecordInstance>
extractRecordInstance(const clang::ClassTemplateSpecializationDecl &node,
                      const clang::SourceManager &sourceManager,
                      FactStore &store) {
  return extractRecord(node, sourceManager)
      .and_then([&](Record record) {
        return extractOpenArguments(node, store)
            .transform([record = std::move(record)](
                           std::vector<TemplateArgument> arguments) mutable {
              RecordInstance instance;
              static_cast<Record &>(instance) = std::move(record);
              instance.templateArguments = std::move(arguments);
              return instance;
            });
      })
      .and_then([&](RecordInstance instance) {
        return extractTemplateParameters(node.getTemplateArgs().asArray(),
                                         node.getASTContext(), store)
            .transform([instance = std::move(instance)](
                           std::vector<TemplateParameter> parameters) mutable {
              instance.templateParameters = std::move(parameters);
              return instance;
            });
      });
}

IndexingResult
storeRecordInstanceRelations(const clang::ClassTemplateSpecializationDecl &node,
                             SymbolId instance, FactStore &store) {
  const auto *pattern = specializedPattern(node);
  if (pattern == nullptr) {
    return std::unexpected(relationFailure(
        "template_instance", "source", node.getQualifiedNameAsString(),
        "target", "<unavailable>", "<unavailable>",
        "template pattern is unavailable"));
  }
  return storeTemplateInstanceRelations(
      instance, node.getQualifiedNameAsString(), *pattern,
      node.getSpecializationKind(), node.getTemplateArgs().asArray(),
      node.getASTContext(), store);
}

} // namespace facts
