#include "platform/PlatformFlags.h"

#include "platform/DriverIncludes.h"
#include "platform/ResourceDirectory.h"

#include <clang/Tooling/CompilationDatabase.h>

#include <cstdio>
#include <filesystem>
#include <optional>
#include <ranges>
#include <utility>

namespace facts {
namespace {

using Commands = std::vector<clang::tooling::CompileCommand>;

std::filesystem::path commandPath(const std::string &directory,
                                  const std::string &value) {
  const std::filesystem::path path(value);
  return (path.is_absolute() ? path : std::filesystem::path(directory) / path)
      .lexically_normal();
}

class PlatformCompilationDatabase final
    : public clang::tooling::CompilationDatabase {
public:
  explicit PlatformCompilationDatabase(Commands commands)
      : commands_(std::move(commands)) {}

  std::vector<clang::tooling::CompileCommand>
  getCompileCommands(llvm::StringRef filePath) const override {
    auto selected = commands_ | std::views::filter([&](const auto &command) {
                      return commandPath(command.Directory, command.Filename) ==
                             commandPath(command.Directory, filePath.str());
                    });
    return {selected.begin(), selected.end()};
  }

  std::vector<std::string> getAllFiles() const override {
    auto files = commands_ | std::views::transform([](const auto &command) {
                   return command.Filename;
                 });
    return {files.begin(), files.end()};
  }

  std::vector<clang::tooling::CompileCommand>
  getAllCompileCommands() const override {
    return commands_;
  }

private:
  Commands commands_;
};

std::expected<Commands, std::string>
selectedCommands(const clang::tooling::CompilationDatabase &database,
                 std::span<const std::string> sources) {
  Commands selected;
  for (const auto &source : sources) {
    auto commands = database.getCompileCommands(source);
    if (commands.empty())
      return std::unexpected("no compile command for source: " + source);
    selected.insert(selected.end(), std::make_move_iterator(commands.begin()),
                    std::make_move_iterator(commands.end()));
  }
  return selected;
}

std::optional<std::filesystem::path> macosSdkRoot() {
#ifdef __APPLE__
  FILE *process = popen("xcrun --show-sdk-path 2>/dev/null", "r");
  if (process == nullptr)
    return std::nullopt;
  char buffer[1024]{};
  const bool read = fgets(buffer, sizeof(buffer), process) != nullptr;
  pclose(process);
  if (!read)
    return std::nullopt;
  std::string value(buffer);
  value.erase(value.find_last_not_of("\r\n") + 1);
  if (!value.starts_with('/'))
    return std::nullopt;
  return std::filesystem::path(value);
#else
  return std::nullopt;
#endif
}

std::expected<Commands, std::string>
configureCommands(Commands commands,
                  const std::filesystem::path &resourceDirectory) {
  Commands configured;
  configured.reserve(commands.size());
  const auto sdkRoot = macosSdkRoot();
  for (auto &command : commands) {
    auto result = platform::configureCommand(std::move(command),
                                             resourceDirectory, sdkRoot);
    if (!result)
      return std::unexpected(result.error());
    configured.push_back(std::move(*result));
  }
  return configured;
}

} // namespace

std::expected<std::unique_ptr<clang::tooling::CompilationDatabase>, std::string>
configurePlatformCompilationDatabase(
    const clang::tooling::CompilationDatabase &database,
    std::span<const std::string> sources) {
  return platform::resolveLinkedResourceDirectory().and_then(
      [&](const auto &resourceDirectory) {
        return selectedCommands(database, sources)
            .and_then([&](auto commands) {
              return configureCommands(std::move(commands), resourceDirectory);
            })
            .transform([](auto commands) {
              return std::unique_ptr<clang::tooling::CompilationDatabase>(
                  std::make_unique<PlatformCompilationDatabase>(
                      std::move(commands)));
            });
      });
}

} // namespace facts
