#ifndef FACTS_TOOL_AST_EXTRACTORS_FILE_H
#define FACTS_TOOL_AST_EXTRACTORS_FILE_H

#include "model/SymbolId.h"

#include <clang/Basic/SourceLocation.h>

#include <expected>
#include <string>
#include <system_error>

namespace clang {
class SourceManager;
}

namespace facts {
class FileManager;

std::expected<std::string, std::error_code>
extractFilePath(const clang::SourceManager &sourceManager, clang::FileID file);

std::expected<FileId, std::error_code>
resolveFile(const clang::SourceManager &sourceManager,
            clang::SourceLocation location, FileManager &files);

} // namespace facts

#endif // FACTS_TOOL_AST_EXTRACTORS_FILE_H
