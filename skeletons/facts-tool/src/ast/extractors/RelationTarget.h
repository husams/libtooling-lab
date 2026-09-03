#pragma once

#include "ast/extractors/Extraction.h"
#include "model/SymbolId.h"

#include <optional>

namespace clang {
class NamedDecl;
class SourceManager;
} // namespace clang

namespace facts {
class FactStore;
class FileManager;

ExtractionResult<std::optional<SymbolId>>
resolveRelationTarget(const clang::NamedDecl &target,
                      const clang::SourceManager &sourceManager,
                      FileManager &files, FactStore &store);

} // namespace facts
