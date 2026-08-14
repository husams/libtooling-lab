#include "ast/extractors/RecordDecl.h"
#include "ast/StoreExtracted.h"

#include "ast/extractors/Definition.h"
#include "ast/extractors/FunctionDecl.h"
#include "ast/extractors/NamedDecl.h"
#include "ast/visitors/SymbolCollector.h"
#include "model/AnySymbol.h"
#include "model/RecordTemplate.h"
#include "model/Relation.h"
#include "storage/FactStore.h"

#include <clang/AST/DeclCXX.h>
#include <clang/AST/DeclTemplate.h>
#include <clang/AST/Type.h>

#include <algorithm>
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

using RelationResult = std::expected<Relation, std::error_code>;
using RelationsResult = std::expected<std::vector<Relation>, std::error_code>;

std::error_code toErrorCode(ExtractionError) {
  return std::make_error_code(std::errc::invalid_argument);
}

RelationResult extractInheritanceRelation(const clang::CXXBaseSpecifier &base,
                                          SymbolId source,
                                          std::uint16_t position,
                                          FactStore &store) {
  const auto *parent = base.getType()->getAsCXXRecordDecl();
  if (!parent) {
    return std::unexpected(
        std::make_error_code(std::errc::operation_not_supported));
  }

  const auto toRelation =
      [&](std::optional<SymbolId> destination) -> RelationResult {
    if (!destination) {
      return std::unexpected(
          std::make_error_code(std::errc::no_such_file_or_directory));
    }

    auto flags = static_cast<std::uint32_t>(base.getAccessSpecifier());
    if (base.isVirtual()) {
      flags |= bit(VirtualBaseBit);
    }
    return Relation{.source = source,
                    .destination = *destination,
                    .kind = RelationKind::Inherits,
                    .flags = static_cast<std::uint16_t>(flags),
                    .position = position};
  };

  return extractUsr(*parent)
      .transform_error(toErrorCode)
      .and_then([&store](std::string usr) { return store.findId(usr); })
      .and_then(toRelation);
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

std::expected<void, std::error_code>
storeInheritanceRelations(const clang::CXXRecordDecl &node, SymbolId source,
                          FactStore &store) {
  auto relationResults =
      std::views::zip(std::views::iota(0U), node.bases()) |
      std::views::transform([&](auto indexedBase) {
        auto [position, base] = indexedBase;
        return extractInheritanceRelation(
            base, source, static_cast<std::uint16_t>(position), store);
      }) |
      std::ranges::to<std::vector>();

  auto failure =
      std::ranges::find_if(relationResults, [](const RelationResult &relation) {
        return !relation;
      });
  if (failure != relationResults.end()) {
    return std::unexpected(failure->error());
  }

  auto relations = relationResults |
                   std::views::transform([](RelationResult &relation) {
                     return std::move(relation).value();
                   }) |
                   std::ranges::to<std::vector>();
  return store.addRelations(relations);
}

} // namespace

void collectSymbol(clang::CXXRecordDecl &node, clang::ASTContext &context,
                   FileManager &files, FactStore &store, bool isDefinition) {
  const auto storeRelations =
      [&](SymbolId source) -> std::expected<void, std::error_code> {
    return isDefinition ? storeInheritanceRelations(node, source, store)
                        : std::expected<void, std::error_code>{};
  };
  if (const auto *templateDeclaration = node.getDescribedClassTemplate()) {
    const auto toTemplate = [&](Record record) {
      return extractTemplateArguments(
                 *templateDeclaration->getTemplateParameters(), store)
          .transform([record = std::move(record)](
                         std::vector<TemplateArgument> arguments) mutable {
            RecordTemplate result;
            static_cast<Record &>(result) = std::move(record);
            result.templateArguments = std::move(arguments);
            return result;
          });
    };

    storeExtracted(node,
                   extractRecord(node, context.getSourceManager()) | toTemplate,
                   context, files, store, storeRelations);
    return;
  }

  storeExtracted(node, extractRecord(node, context.getSourceManager()), context,
                 files, store, storeRelations);
}

} // namespace facts
