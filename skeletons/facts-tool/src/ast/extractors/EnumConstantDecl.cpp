#include "ast/extractors/EnumConstantDecl.h"

#include "ast/StoreExtracted.h"
#include "ast/extractors/Definition.h"
#include "ast/extractors/Initializer.h"
#include "ast/extractors/NamedDecl.h"
#include "model/Relation.h"

#include <clang/AST/ASTContext.h>
#include <clang/AST/Decl.h>
#include <llvm/ADT/SmallString.h>
#include <llvm/Support/Casting.h>

#include <array>
#include <optional>
#include <system_error>
#include <utility>

namespace facts {
namespace {

std::string enumeratorValue(const clang::EnumConstantDecl &node) {
  llvm::SmallString<32> value;
  node.getInitVal().toString(value, 10);
  return std::string{value};
}

ExtractionResult<std::optional<std::string>>
extractInitializerExpression(const clang::EnumConstantDecl &node,
                             const clang::SourceManager &sourceManager) {
  const auto *expression = node.getInitExpr();
  if (expression == nullptr) {
    return std::nullopt;
  }
  auto initializer = extractInitializer(expression, node.getType(),
                                        node.getASTContext(), sourceManager);
  return initializer
             ? std::optional<std::string>{std::move(initializer->expression)}
             : std::nullopt;
}

ExtractionResult<Enumerator>
addEnumeratorDetails(Enumerator enumerator, const clang::EnumConstantDecl &node,
                     const clang::SourceManager &sourceManager) {
  return extractInitializerExpression(node, sourceManager)
      .transform([enumerator = std::move(enumerator),
                  &node](std::optional<std::string> expression) mutable {
        enumerator.value = enumeratorValue(node);
        enumerator.initializerExpression = std::move(expression);
        return std::move(enumerator);
      });
}

std::expected<SymbolId, std::error_code>
enumerationId(const clang::EnumConstantDecl &node, FactStore &store) {
  const auto *enumeration =
      llvm::dyn_cast<clang::EnumDecl>(node.getDeclContext());
  if (enumeration == nullptr) {
    return std::unexpected(std::make_error_code(std::errc::invalid_argument));
  }
  return extractUsr(*enumeration)
      .transform_error([](ExtractionError) {
        return std::make_error_code(std::errc::invalid_argument);
      })
      .and_then([&store](std::string usr) { return store.findId(usr); })
      .and_then([](std::optional<SymbolId> id)
                    -> std::expected<SymbolId, std::error_code> {
        return id ? std::expected<SymbolId, std::error_code>{*id}
                  : std::unexpected(std::make_error_code(
                        std::errc::no_such_file_or_directory));
      });
}

std::expected<void, std::error_code>
storeEnumerationRelation(const clang::EnumConstantDecl &node,
                         SymbolId enumerator, FactStore &store) {
  return enumerationId(node, store).and_then([&](SymbolId enumeration) {
    const std::array relations{Relation{
        .source = enumeration,
        .destination = enumerator,
        .kind = RelationKind::Contains,
        .flags = static_cast<std::uint16_t>(bit(LexicalBit)),
    }};
    return store.addRelations(relations);
  });
}

} // namespace

ExtractionResult<Enumerator>
extractEnumerator(const clang::EnumConstantDecl &node,
                  const clang::SourceManager &sourceManager) {
  const auto toEnumerator = [](Symbol symbol) -> ExtractionResult<Enumerator> {
    return toSymbolModel<Enumerator>(std::move(symbol));
  };
  const auto addDetails = [&](Enumerator enumerator) {
    return addEnumeratorDetails(std::move(enumerator), node, sourceManager);
  };
  const auto addDefinition = [&](Enumerator enumerator) {
    return addDefinitionRegion(std::move(enumerator), node, true,
                               sourceManager);
  };

  return extractSymbol<Symbol, clang::NamedDecl>(node, sourceManager)
      .and_then(toEnumerator)
      .and_then(addDetails)
      .and_then(addDefinition);
}

IndexingResult collectSymbol(clang::EnumConstantDecl &node,
                             clang::ASTContext &context, FileManager &files,
                             FactStore &store) {
  const auto storeRelation = [&](SymbolId enumerator) {
    return storeEnumerationRelation(node, enumerator, store);
  };
  return storeExtracted(node,
                        extractEnumerator(node, context.getSourceManager()),
                        context, files, store, storeRelation);
}

} // namespace facts
