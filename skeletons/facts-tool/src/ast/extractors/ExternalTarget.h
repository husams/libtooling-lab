#pragma once

#include "ast/extractors/CallableProperties.h"
#include <clang/AST/Decl.h>
#include <clang/Basic/SourceManager.h>
#include <clang/Index/IndexSymbol.h>

namespace facts {

inline const clang::NamedDecl &
visibleTarget(const clang::NamedDecl &target,
              const clang::SourceManager &sourceManager) {
  const auto usable = [&](const clang::NamedDecl &decl) {
    return sourceManager.getExpansionLoc(decl.getLocation()).isValid();
  };
  if (usable(target))
    return target;
  if (const auto *function = llvm::dyn_cast<clang::FunctionDecl>(&target)) {
    for (const auto *decl : function->redecls()) {
      if (usable(*decl))
        return *decl;
    }
  }
  return *target.getMostRecentDecl();
}

inline bool compilerProvided(const clang::NamedDecl &target,
                             const clang::SourceManager &sourceManager) {
  return llvm::isa<clang::FunctionDecl>(target) && target.isImplicit() &&
         sourceManager.getExpansionLoc(target.getLocation()).isInvalid();
}

inline std::expected<Symbol, std::error_code>
externalSymbol(const clang::NamedDecl &target, const std::string &usr,
               bool compiler) {
  Function symbol{};
  static_cast<clang::index::SymbolInfo &>(symbol) =
      clang::index::getSymbolInfo(&target);
  symbol.usr = usr;
  symbol.qualifiedName = target.getQualifiedNameAsString();
  symbol.flags = bit(ExternalBit);
  if (compiler)
    return addCallableProperties(std::move(symbol),
                                 llvm::cast<clang::FunctionDecl>(target))
        .transform([](Function value) { return Symbol{std::move(value)}; })
        .transform_error([](ExtractionError) {
          return std::make_error_code(std::errc::invalid_argument);
        });
  return symbol;
}

} // namespace facts
