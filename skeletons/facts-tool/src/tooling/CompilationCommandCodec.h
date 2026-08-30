#pragma once

#include "tooling/StoredCompilationReader.h"

#include <clang/Tooling/CompilationDatabase.h>

#include <cstddef>
#include <expected>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace facts {

using CompileCommands = std::vector<clang::tooling::CompileCommand>;

inline constexpr std::string_view storedSourceArgumentMarker =
    "\x1f"
    "facts-tool-source-argument";

std::filesystem::path normalizeCompilationPath(std::filesystem::path path);

std::filesystem::path logicalCompilationPath(std::filesystem::path path);

std::size_t commandStart(const std::vector<std::string> &arguments);

std::vector<std::string>
sanitizeCommandArguments(std::vector<std::string> arguments);

std::string defaultCompilerDriver(const std::filesystem::path &source);

std::string encodeCompileOptions(const std::vector<std::string> &options);

std::expected<std::vector<std::string>, std::string>
decodeCompileOptions(std::string_view text);

std::expected<clang::tooling::CompileCommand, std::string>
decodeStoredCommand(const StoredCompileFile &file,
                    const StoredCommandAliases &aliases);

std::expected<CompileCommands, std::string>
decodeCompileCommands(const StoredCompilationSnapshot &snapshot);

} // namespace facts
