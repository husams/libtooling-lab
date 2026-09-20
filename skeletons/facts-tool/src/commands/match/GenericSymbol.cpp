#include "commands/match/GenericSymbol.h"
#include "ast/extractors/NamedDecl.h"
#include "ast/visitors/SymbolCollector.h"
#include "storage/FactStore.h"
#include "storage/SemanticProperties.h"
#include <clang/AST/ASTContext.h>
#include <clang/AST/Decl.h>

namespace facts::commands::match {
std::expected<std::optional<PersistedSymbol>, std::string>
persistAnySymbol(const clang::NamedDecl &node, clang::ASTContext &context,
                 FileManager &files, FactStore &store) {
  auto usr = extractUsr(node);
  if (!usr || node.getLocation().isInvalid()) return std::nullopt;
  if (supportsSymbol(node))
    return persistSymbol(node, context, files, store)
        .transform([](PersistedSymbol symbol) { return std::optional{std::move(symbol)}; });
  return collectDeclaredSymbol(const_cast<clang::NamedDecl &>(node), context, files, store)
      .transform_error([](const IndexingError &error) { return error.message; })
      .and_then([&] {
        return store.findId(*usr).transform_error([](auto error) { return error.message(); });
      }).transform([&](std::optional<SymbolId> id) -> std::optional<PersistedSymbol> {
        if (!id || id->file == builtinFileId) return std::nullopt;
        const auto name = extractQualifiedName(node, context.getSourceManager());
        return PersistedSymbol{*id, node.getDeclKindName(), name,
            MatchedSymbol{*usr, name, id->file,
                storage::storedSymbolKind(clang::index::getSymbolInfo(&node).Kind)}, ""};
      });
}
}
