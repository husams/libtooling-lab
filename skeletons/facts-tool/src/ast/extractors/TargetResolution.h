#ifndef FACTS_TOOL_AST_EXTRACTORS_TARGETRESOLUTION_H
#define FACTS_TOOL_AST_EXTRACTORS_TARGETRESOLUTION_H

#include "model/SymbolId.h"

#include <expected>
#include <string>
#include <system_error>

namespace clang {
class NamedDecl;
class SourceManager;
} // namespace clang

namespace facts {
class FactStore;
class FileManager;

std::expected<SymbolId, std::error_code> findOrStoreSymbolTarget(
    const clang::NamedDecl &target, const clang::SourceManager &sourceManager,
    FileManager &files, FactStore &store, const std::string &usr);

} // namespace facts

#endif // FACTS_TOOL_AST_EXTRACTORS_TARGETRESOLUTION_H
