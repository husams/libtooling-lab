#pragma once

#include "ast/Indexing.h"
#include "model/SymbolId.h"

namespace clang {
class FunctionDecl;
}

namespace facts {
class FactStore;
class FileManager;

IndexingResult storeReturnType(const clang::FunctionDecl &node,
                               SymbolId callable, FileManager &files,
                               FactStore &store);
} // namespace facts
