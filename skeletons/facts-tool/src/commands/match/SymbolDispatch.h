#pragma once

#include "model/SymbolId.h"

#include <expected>
#include <string>

namespace clang {
class ASTContext;
class NamedDecl;
} // namespace clang

namespace facts {
class FactStore;
class FileManager;
} // namespace facts

namespace facts::commands::match {
struct PersistedSymbol {
  SymbolId id;
  std::string kind;
  std::string name;
};

std::expected<PersistedSymbol, std::string>
persistSymbol(const clang::NamedDecl &node, clang::ASTContext &context,
              FileManager &files, FactStore &store);
} // namespace facts::commands::match
