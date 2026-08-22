#include "ast/extractors/NamedDecl.h"

#include "ast/extractors/Location.h"
#include "ast/extractors/TemplatePattern.h"
#include "ast/extractors/Type.h"

#include "clang/AST/Decl.h"
#include "clang/AST/DeclTemplate.h"
#include "clang/Index/IndexSymbol.h"
#include "clang/Index/USRGeneration.h"

#include <llvm/ADT/SmallString.h>
#include <llvm/Support/Casting.h>

#include <string>
#include <vector>

namespace facts {

ExtractionResult<std::string> extractUsr(const clang::NamedDecl &node) {
  llvm::SmallString<128> usr;
  if (clang::index::generateUSRForDecl(&node, usr)) {
    return std::unexpected(ExtractionError::InvalidUsr);
  }
  return std::string{usr};
}

TypeResult extractAliasTarget(const clang::TypedefNameDecl &node,
                              const clang::SourceManager &sourceManager,
                              FileManager &files, FactStore &store) {
  return extractType(node.getUnderlyingType(), sourceManager, files, store);
}

ExtractionResult<std::vector<TemplateArgument>>
extractAliasTemplateArguments(const clang::TypedefNameDecl &node,
                              const clang::SourceManager &sourceManager,
                              FileManager &files, FactStore &store) {
  const auto *alias = llvm::dyn_cast<clang::TypeAliasDecl>(&node);
  if (alias == nullptr || alias->getDescribedAliasTemplate() == nullptr) {
    return std::vector<TemplateArgument>{};
  }
  return extractTemplateArguments(
      *alias->getDescribedAliasTemplate()->getTemplateParameters(),
      sourceManager, files, store);
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
