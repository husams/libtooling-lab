#pragma once
#include "commands/match/SymbolDispatch.h"
namespace facts::commands::match {
std::expected<std::optional<PersistedSymbol>, std::string>
persistAnySymbol(const clang::NamedDecl &node, clang::ASTContext &context,
                 FileManager &files, FactStore &store);
}
