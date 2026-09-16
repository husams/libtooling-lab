#include "tooling/astcache/Metadata.h"
#include "tooling/astcache/FileIdentity.h"
#include "tooling/astcache/FingerprintArguments.h"

#include <clang/Basic/Version.h>
#include <clang/Tooling/ArgumentsAdjusters.h>
#include <clang/Tooling/CompilationDatabase.h>
#include <cstdlib>
#include <string_view>

namespace facts::astcache::detail {
namespace {
clang::tooling::CommandLineArguments
parseArguments(const clang::tooling::CompileCommand &command,
                bool clearAdjusters) {
  if (clearAdjusters)
    return command.CommandLine;
  // Match ClangTool's default adjuster order. Import removes -c and output
  // switches when persisting commands, but those switches do not affect ASTs.
  using namespace clang::tooling;
  const auto adjust = combineAdjusters(
      combineAdjusters(getClangStripOutputAdjuster(),
                       getClangSyntaxOnlyAdjuster()),
      getClangStripDependencyFileAdjuster());
  return adjust(command.CommandLine, command.Filename);
}

std::string entryKey(const clang::tooling::CompileCommand &command,
                     const fs::path &cwd, const fs::path &source,
                     bool clearAdjusters) {
  std::string bytes;
  const auto append = [&](std::string_view value) {
    bytes += std::to_string(value.size());
    bytes += ':';
    bytes += value;
  };
  append("project-db-git-commit-v1");
  append(clang::getClangFullVersion());
  append(source.string());
  append(cwd.string());
  append(source.string());
  append(clearAdjusters ? "clear" : "default");
  append(std::to_string(command.CommandLine.size()));
  for (const auto &argument : fingerprintArguments(command.CommandLine, source, cwd))
    append(argument);
  for (const auto *name : {"CPATH", "C_INCLUDE_PATH", "CPLUS_INCLUDE_PATH",
                           "OBJC_INCLUDE_PATH", "SDKROOT", "PATH",
                           "MACOSX_DEPLOYMENT_TARGET", "SOURCE_DATE_EPOCH"}) {
    append(name);
    append(std::getenv(name) ? std::getenv(name) : "");
  }
  return digest(bytes);
}
} // namespace

std::expected<Entry, std::string>
locateEntry(const clang::tooling::CompilationDatabase &database,
            const std::string &source, const Options &options,
            bool clearAdjusters) {
  if (options.directory.empty())
    return std::unexpected("AST cache directory is empty");
  if (options.database.empty())
    return std::unexpected("AST cache project database is empty");
  std::error_code error;
  const auto cacheDirectory = fs::absolute(options.directory, error);
  if (error)
    return std::unexpected(error.message());
  const auto projectDatabase = fs::absolute(options.database, error);
  if (error)
    return std::unexpected(error.message());
  const auto commands = database.getCompileCommands(source);
  if (commands.size() != 1)
    return std::unexpected("AST caching requires one compile command per TU");
  auto command = commands.front();
  command.CommandLine = parseArguments(command, clearAdjusters);
  const auto cwd = fs::absolute(command.Directory, error);
  if (error)
    return std::unexpected(error.message());
  const auto input = resolve(source, cwd);
  const auto name = entryKey(command, cwd, input, clearAdjusters);
  return Entry{cacheDirectory / (name + ".ast"), projectDatabase, input, cwd,
               name};
}
} // namespace facts::astcache::detail
