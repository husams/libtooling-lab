#include "tooling/StoredCompilationDatabase.h"

#include "tooling/CompilationCommandCodec.h"
#include "tooling/StoredCompilationReader.h"

#include <filesystem>
#include <ranges>
#include <utility>
#include <vector>

namespace facts {
namespace {

class StoredCompilationDatabase final
    : public clang::tooling::CompilationDatabase {
public:
  explicit StoredCompilationDatabase(CompileCommands commands)
      : commands_(std::move(commands)) {}

  std::vector<clang::tooling::CompileCommand>
  getCompileCommands(llvm::StringRef filePath) const override {
    const auto identity = normalizeCompilationPath(filePath.str()).string();
    auto matching =
        commands_ | std::views::filter([&](const auto &command) {
          return normalizeCompilationPath(command.Filename).string() ==
                 identity;
        });
    return matching | std::ranges::to<CompileCommands>();
  }

  std::vector<std::string> getAllFiles() const override {
    auto files = commands_ | std::views::transform([](const auto &command) {
                   return command.Filename;
                 });
    return files | std::ranges::to<std::vector>();
  }

  CompileCommands getAllCompileCommands() const override { return commands_; }

private:
  CompileCommands commands_;
};

std::unique_ptr<clang::tooling::CompilationDatabase>
makeStoredCompilationDatabase(CompileCommands commands) {
  return std::make_unique<StoredCompilationDatabase>(std::move(commands));
}

} // namespace

std::expected<std::unique_ptr<clang::tooling::CompilationDatabase>, std::string>
loadStoredCompilationDatabase(std::string databasePath,
                              std::span<const std::string> requestedSources) {
  return openStoredDatabase(normalizeCompilationPath(std::move(databasePath)))
      .and_then([requestedSources](StoredDatabase database) {
        return readStoredCompilation(database.get(), requestedSources);
      })
      .and_then(decodeCompileCommands)
      .transform(makeStoredCompilationDatabase);
}

} // namespace facts
