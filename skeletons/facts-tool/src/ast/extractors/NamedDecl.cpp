#include "ast/extractors/NamedDecl.h"

#include "ast/extractors/Location.h"

#include "clang/AST/Decl.h"
#include "clang/Index/IndexSymbol.h"
#include "clang/Index/USRGeneration.h"

#include <llvm/ADT/SmallString.h>

#include <string>

namespace facts {

ExtractionResult<std::string> extractUsr(const clang::NamedDecl &node) {
  llvm::SmallString<128> usr;
  if (clang::index::generateUSRForDecl(&node, usr)) {
    return std::unexpected(ExtractionError::InvalidUsr);
  }
  return std::string{usr};
}

template <>
ExtractionResult<Symbol> extractSymbol<Symbol, clang::NamedDecl>(
    const clang::NamedDecl &node, const clang::SourceManager &sourceManager) {
  const auto toSymbol = [&](Location location) {
    return extractUsr(node).transform([&](std::string usr) {
      Symbol symbol;
      static_cast<clang::index::SymbolInfo &>(symbol) =
          clang::index::getSymbolInfo(&node);
      symbol.usr = std::move(usr);
      symbol.qualifiedName = node.getQualifiedNameAsString();
      symbol.loc = location;
      return symbol;
    });
  };

  return extractLocation(sourceManager, node.getLocation()) | toSymbol;
}

} // namespace facts
