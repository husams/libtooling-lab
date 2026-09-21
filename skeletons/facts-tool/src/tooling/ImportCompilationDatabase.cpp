#include "tooling/ImportCompilationDatabase.h"

#include "tooling/StoredCompilationDatabase.h"
#include <clang/Tooling/JSONCompilationDatabase.h>
#include <llvm/Support/VirtualFileSystem.h>

namespace facts {
namespace {
std::filesystem::path resolve(const std::filesystem::path &directory,
                              const std::filesystem::path &path) {
  return (path.is_absolute() ? path : directory / path).lexically_normal();
}
}

std::expected<std::unique_ptr<clang::tooling::CompilationDatabase>, std::string>
loadImportCompilationDatabase(const std::filesystem::path &directory) {
  const auto absolute = std::filesystem::absolute(directory).lexically_normal();
  const auto path = absolute.filename() == "compile_commands.json"
                        ? absolute : absolute / "compile_commands.json";
  std::string error;
  // Retain the CLI's compile_flags.txt fallback when there is no JSON file.
  // Fixed databases have no enumerated sources and are queried by selector.
  if (!std::filesystem::exists(path)) {
    auto fallback = clang::tooling::CompilationDatabase::loadFromDirectory(
        path.parent_path().string(), error);
    if (fallback) return fallback;
    return std::unexpected("cannot load compilation database " + path.string() +
                           ": " + error);
  }
  auto database = clang::tooling::JSONCompilationDatabase::loadFromFile(
      path.string(), error, clang::tooling::JSONCommandLineSyntax::AutoDetect);
  if (!database)
    return std::unexpected("cannot load compilation database " + path.string() +
                           ": " + error);
  auto commands = database->getAllCompileCommands();
  for (auto &command : commands) {
    command.Directory = resolve(path.parent_path(), command.Directory).string();
    command.Filename = resolve(command.Directory, command.Filename).string();
  }
  return clang::tooling::inferTargetAndDriverMode(
      clang::tooling::inferMissingCompileCommands(
          clang::tooling::expandResponseFiles(
              makeStoredCompilationDatabase(std::move(commands)),
              llvm::vfs::createPhysicalFileSystem())));
}
}
