#include "tooling/StoredCompilationDatabase.h"

#include "tooling/CompilationCommandCodec.h"
#include "tooling/CompilationContext.h"
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
    const auto logical = logicalCompilationPath(filePath.str());
    auto exact = commands_ | std::views::filter([&](const auto &command) {
      return logicalCompilationPath(command.Filename) == logical;
    }) | std::ranges::to<CompileCommands>();
    // Distinct logical paths can share a physical file and still carry
    // different build options. Only use physical identity as a fallback.
    if (!exact.empty()) return exact;
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

} // namespace

std::unique_ptr<clang::tooling::CompilationDatabase>
makeStoredCompilationDatabase(CompileCommands commands) {
  return std::make_unique<StoredCompilationDatabase>(std::move(commands));
}

std::expected<std::unique_ptr<clang::tooling::CompilationDatabase>, std::string>
loadStoredCompilationDatabase(std::string databasePath,
                              std::span<const std::string> requestedSources) {
  const auto project = normalizeCompilationPath(std::move(databasePath));
  if (const auto *context = invocationCompilation(project)) {
    CompileCommands selected;
    for (const auto &command : context->commands) {
      const bool requested = requestedSources.empty() ||
          std::ranges::any_of(requestedSources, [&](const auto &source) {
            return normalizeCompilationPath(source) ==
                   normalizeCompilationPath(command.Filename);
          });
      if (requested) selected.push_back(command);
    }
    if (!selected.empty()) return makeStoredCompilationDatabase(std::move(selected));
  }
  return openStoredDatabase(project)
      .and_then([requestedSources](StoredDatabase database) {
        return readStoredCompilation(database.get(), requestedSources);
      })
      .and_then(decodeCompileCommands)
      .transform(makeStoredCompilationDatabase);
}

} // namespace facts
