#include "ast/extractors/RecordDecl.h"
#include "ast/StoreExtracted.h"

#include "ast/extractors/Definition.h"
#include "ast/extractors/NamedDecl.h"
#include "ast/extractors/RecordInstance.h"
#include "ast/extractors/TargetResolution.h"
#include "ast/extractors/TemplatePattern.h"
#include "ast/visitors/SymbolCollector.h"
#include "model/AnySymbol.h"
#include "model/RecordTemplate.h"
#include "model/Relation.h"
#include "storage/FactStore.h"

#include <clang/AST/DeclCXX.h>
#include <clang/AST/DeclTemplate.h>
#include <clang/AST/Type.h>
#include <clang/Basic/SourceManager.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <expected>
#include <optional>
#include <ranges>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace facts {
namespace {

struct InheritanceRelation {
  Relation relation;
  std::string baseName;
  std::string usr;
};

using RelationResult =
    std::expected<std::optional<InheritanceRelation>, IndexingError>;

IndexingError inheritanceFailure(std::string_view derived,
                                 std::string_view base, std::string_view usr,
                                 std::string_view detail) {
  return relationFailure("inheritance", "derived", derived, "base", base, usr,
                         detail);
}

RelationResult extractInheritanceRelation(
    const clang::CXXBaseSpecifier &base, SymbolId source,
    std::uint16_t position, const clang::SourceManager &sourceManager,
    FileManager &files, FactStore &store, std::string_view derivedName) {
  const auto *parent = base.getType()->getAsCXXRecordDecl();
  if (!parent) {
    if (base.getType()->isDependentType()) {
      return std::optional<InheritanceRelation>{};
    }
    return std::unexpected(
        inheritanceFailure(derivedName, base.getType().getAsString(),
                           "<unavailable>", "base declaration is unavailable"));
  }

  const auto baseName = parent->getQualifiedNameAsString();
  const auto toRelation = [&](SymbolId destination,
                              std::string usr) -> InheritanceRelation {
    auto flags = static_cast<std::uint32_t>(base.getAccessSpecifier());
    if (base.isVirtual()) {
      flags |= bit(VirtualBaseBit);
    }
    return InheritanceRelation{
        .relation = Relation{.source = source,
                             .destination = destination,
                             .kind = RelationKind::Inherits,
                             .flags = static_cast<std::uint16_t>(flags),
                             .position = position},
        .baseName = baseName,
        .usr = std::move(usr)};
  };

  return extractUsr(*parent)
      .transform_error([&](ExtractionError error) {
        return inheritanceFailure(derivedName, baseName, "<unavailable>",
                                  extractionErrorName(error));
      })
      .and_then([&](std::string usr) -> RelationResult {
        return findOrStoreSymbolTarget(*parent, sourceManager, files, store,
                                       usr)
            .transform_error([&](std::error_code error) {
              return inheritanceFailure(derivedName, baseName, usr,
                                        error.message());
            })
            .transform([&](SymbolId destination) {
              return std::optional<InheritanceRelation>{
                  toRelation(destination, std::move(usr))};
            });
      });
}

} // namespace

ExtractionResult<Record>
extractRecord(const clang::CXXRecordDecl &node,
              const clang::SourceManager &sourceManager) {
  const auto toRecord = [](Symbol symbol) -> ExtractionResult<Record> {
    return toSymbolModel<Record>(std::move(symbol));
  };
  const auto addDefinition = [&](Record record) {
    return addDefinitionRegion(std::move(record), node,
                               node.isThisDeclarationADefinition(),
                               sourceManager);
  };

  return extractSymbol<Symbol, clang::NamedDecl>(node, sourceManager) |
         toRecord | addDefinition;
}

namespace {

IndexingResult
storeInheritanceRelations(const clang::CXXRecordDecl &node, SymbolId source,
                          const clang::SourceManager &sourceManager,
                          FileManager &files, FactStore &store) {
  const auto derivedName = node.getQualifiedNameAsString();
  auto relationResults =
      std::views::zip(std::views::iota(0U), node.bases()) |
      std::views::transform([&](auto indexedBase) {
        auto [position, base] = indexedBase;
        return extractInheritanceRelation(
            base, source, static_cast<std::uint16_t>(position), sourceManager,
            files, store, derivedName);
      }) |
      std::ranges::to<std::vector>();

  auto failure =
      std::ranges::find_if(relationResults, [](const RelationResult &relation) {
        return !relation;
      });
  if (failure != relationResults.end()) {
    return std::unexpected(failure->error());
  }

  auto relations =
      relationResults |
      std::views::transform([](const RelationResult &relation) -> const auto & {
        return relation.value();
      }) |
      std::views::filter(
          [](const auto &relation) { return relation.has_value(); }) |
      std::views::transform([](const auto &relation) { return *relation; }) |
      std::ranges::to<std::vector>();

  const auto persist = [&](IndexingResult result,
                           const InheritanceRelation &relation) {
    return std::move(result).and_then([&] {
      const std::array values{relation.relation};
      return store.addRelations(values).transform_error(
          [&](std::error_code error) {
            return inheritanceFailure(derivedName, relation.baseName,
                                      relation.usr, error.message());
          });
    });
  };
  return std::ranges::fold_left(relations, IndexingResult{}, persist);
}

} // namespace

IndexingResult collectSymbol(clang::CXXRecordDecl &node,
                             clang::ASTContext &context, FileManager &files,
                             FactStore &store, bool isDefinition) {
  const auto storeRelations = [&](SymbolId source) -> IndexingResult {
    return isDefinition ? storeInheritanceRelations(node, source,
                                                    context.getSourceManager(),
                                                    files, store)
                        : IndexingResult{};
  };

  if (const auto *instance =
          llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(&node)) {
    const auto storeInstanceRelations = [&](SymbolId source) -> IndexingResult {
      return storeRelations(source).and_then([&] {
        return withContext(
            storeRecordInstanceRelations(*instance, source, files, store),
            "cannot persist record-instance relations for '" +
                node.getQualifiedNameAsString() + "'");
      });
    };

    return storeExtracted(node,
                          extractRecordInstance(*instance,
                                                context.getSourceManager(),
                                                files, store),
                          context, files, store, storeInstanceRelations);
  }

  if (const auto *templateDeclaration = node.getDescribedClassTemplate()) {
    const auto toTemplate = [&](Record record) {
      return extractTemplateArguments(
                 *templateDeclaration->getTemplateParameters(),
                 context.getSourceManager(), files, store)
          .transform([record = std::move(record)](
                         std::vector<TemplateArgument> arguments) mutable {
            RecordTemplate result;
            static_cast<Record &>(result) = std::move(record);
            result.templateArguments = std::move(arguments);
            return result;
          });
    };

    return storeExtracted(
        node, extractRecord(node, context.getSourceManager()) | toTemplate,
        context, files, store, storeRelations);
  }

  return storeExtracted(node, extractRecord(node, context.getSourceManager()),
                        context, files, store, storeRelations);
}

} // namespace facts
