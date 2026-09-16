#include "tooling/astcache/Metadata.h"
#include "tooling/astcache/ExternalInputs.h"
#include "tooling/astcache/FileIdentity.h"

#include <clang/Basic/Version.h>
#include <clang/Tooling/ArgumentsAdjusters.h>
#include <clang/Tooling/CompilationDatabase.h>
#include <cstdlib>

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
                     bool clearAdjusters, llvm::json::Array external) {
  llvm::json::Array arguments;
  for (const auto &argument : command.CommandLine)
    arguments.push_back(argument);
  llvm::json::Object environment;
  for (const auto *name : {"CPATH", "C_INCLUDE_PATH", "CPLUS_INCLUDE_PATH",
                           "OBJC_INCLUDE_PATH", "SDKROOT", "PATH",
                           "MACOSX_DEPLOYMENT_TARGET", "SOURCE_DATE_EPOCH"})
    environment[name] = std::getenv(name) ? std::getenv(name) : "";
  return digest(serialize(llvm::json::Object{
      {"schema", Schema}, {"compiler", clang::getClangFullVersion()},
      {"source", source.string()}, {"directory", cwd.string()},
      {"filename", command.Filename},
      {"arguments", std::move(arguments)}, {"external_inputs", std::move(external)},
      {"environment", std::move(environment)}, {"clear_adjusters", clearAdjusters}}));
}
} // namespace

std::expected<Entry, std::string>
locateEntry(const clang::tooling::CompilationDatabase &database,
            const std::string &source, const fs::path &directory,
            bool clearAdjusters) {
  if (directory.empty())
    return std::unexpected("AST cache directory is empty");
  const auto cacheDirectory = identity(directory);
  if (!cacheDirectory)
    return std::unexpected(cacheDirectory.error());
  const auto commands = database.getCompileCommands(source);
  if (commands.size() != 1)
    return std::unexpected("AST caching requires one compile command per TU");
  auto command = commands.front();
  command.CommandLine = parseArguments(command, clearAdjusters);
  return identity(command.Directory).and_then([&](const fs::path &cwd) {
    return identity(resolve(source, cwd)).and_then([&](const fs::path &input) {
      return externalInputs(command, cwd)
          .transform([&](llvm::json::Array external) {
            const auto name = entryKey(command, cwd, input, clearAdjusters,
                                       std::move(external));
            return Entry{*cacheDirectory / (name + ".ast"),
                         *cacheDirectory / (name + ".json"), input, cwd, {}};
          });
    });
  });
}
} // namespace facts::astcache::detail
