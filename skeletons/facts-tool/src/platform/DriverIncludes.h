#ifndef FACTS_TOOL_DRIVERINCLUDES_H
#define FACTS_TOOL_DRIVERINCLUDES_H

#include <clang/Tooling/CompilationDatabase.h>

#include <expected>
#include <filesystem>
#include <optional>
#include <string>

namespace facts::platform {

std::expected<clang::tooling::CompileCommand, std::string>
configureCommand(clang::tooling::CompileCommand command,
                 const std::filesystem::path &resourceDirectory,
                 const std::optional<std::filesystem::path> &sdkRoot);

} // namespace facts::platform

#endif // FACTS_TOOL_DRIVERINCLUDES_H
