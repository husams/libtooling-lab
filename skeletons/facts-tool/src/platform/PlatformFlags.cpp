#include "platform/PlatformFlags.h"
#include "tooling/ExecutionTrace.h"

#include "platform/DriverIncludes.h"
#include "platform/ResourceDirectory.h"

#include <clang/Basic/Version.h>
#include <clang/Tooling/CompilationDatabase.h>

#include <cstdio>
#include <filesystem>
#include <optional>
#include <ranges>
#include <string>
#include <unordered_map>
#include <utility>

namespace facts {
namespace {

using Commands = std::vector<clang::tooling::CompileCommand>;

std::filesystem::path commandPath(const std::filesystem::path &directory,
                                  const std::string &value) {
  const std::filesystem::path path(value);
  return (path.is_absolute() ? path : std::filesystem::path(directory) / path)
      .lexically_normal();
}

std::filesystem::path physicalPath(const std::filesystem::path &path) {
  std::error_code error;
  const auto canonical = std::filesystem::weakly_canonical(path, error);
  return error ? path : canonical;
}

class PlatformCompilationDatabase final
    : public clang::tooling::CompilationDatabase {
public:
  explicit PlatformCompilationDatabase(Commands commands)
      : commands_(std::move(commands)),
        invocationDirectory_(std::filesystem::current_path()) {
    for (std::size_t index = 0; index < commands_.size(); ++index) {
      const auto &command = commands_[index];
      const auto directory = commandPath(invocationDirectory_, command.Directory);
      const auto source = commandPath(directory, command.Filename);
      logicalCommands_[source.string()].push_back(index);
      physicalCommands_[physicalPath(source).string()].push_back(index);
    }
  }

  std::vector<clang::tooling::CompileCommand>
  getCompileCommands(llvm::StringRef filePath) const override {
    // A source selector belongs to the caller's working directory, whereas
    // a compile command's relative filename belongs to its build directory.
    const auto source = commandPath(invocationDirectory_, filePath.str());
    if (const auto found = logicalCommands_.find(source.string());
        found != logicalCommands_.end())
      return commandsAt(found->second);
    // Prefer the stored logical owner when two registered paths happen to
    // point at the same physical file with different compiler contexts.
    const auto found = physicalCommands_.find(physicalPath(source).string());
    return found == physicalCommands_.end() ? Commands{}
                                            : commandsAt(found->second);
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
  Commands commandsAt(const std::vector<std::size_t> &indices) const {
    auto selected = indices | std::views::transform(
                                  [&](auto index) { return commands_[index]; });
    return {selected.begin(), selected.end()};
  }

  Commands commands_;
  std::filesystem::path invocationDirectory_;
  std::unordered_map<std::string, std::vector<std::size_t>> logicalCommands_;
  std::unordered_map<std::string, std::vector<std::size_t>> physicalCommands_;
};

std::expected<Commands, std::string>
selectedCommands(const clang::tooling::CompilationDatabase &database,
                 std::span<const std::string> sources) {
  Commands selected;
  std::unordered_map<std::string, std::vector<std::size_t>> selectedFiles;
  for (const auto &source : sources) {
    auto commands = database.getCompileCommands(source);
    if (commands.empty())
      return std::unexpected("no compile command for source: " + source);
    for (auto &command : commands) {
      auto &indices = selectedFiles[command.Filename];
      if (std::ranges::any_of(indices, [&](auto index) {
            return selected[index] == command;
          }))
        continue;
      indices.push_back(selected.size());
      selected.push_back(std::move(command));
    }
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
                  const std::filesystem::path &resourceDirectory,
                  const astcache::Options &cache) {
  Commands configured;
  configured.reserve(commands.size());
  const auto sdkRoot = macosSdkRoot();
  for (auto &command : commands) {
    auto result = platform::configureCommand(std::move(command),
                                             resourceDirectory, sdkRoot, cache);
    if (!result)
      return std::unexpected(result.error());
    traceCommand(result->Filename, result->Directory, result->CommandLine);
    configured.push_back(std::move(*result));
  }
  return configured;
}

} // namespace

std::expected<std::unique_ptr<clang::tooling::CompilationDatabase>, std::string>
configurePlatformCompilationDatabase(
    const clang::tooling::CompilationDatabase &database,
    std::span<const std::string> sources,
    const astcache::Options &cache) {
  return platform::resolveLinkedResourceDirectory().and_then(
      [&](const auto &resourceDirectory) {
        return selectedCommands(database, sources)
            .and_then([&](auto commands) {
              return configureCommands(std::move(commands), resourceDirectory, cache);
            })
            .transform([](auto commands) {
              return std::unique_ptr<clang::tooling::CompilationDatabase>(
                  std::make_unique<PlatformCompilationDatabase>(
                      std::move(commands)));
            });
      });
}

std::string platformFingerprint() {
  auto resourceDirectory = platform::resolveLinkedResourceDirectory();
  const auto sdkRoot = macosSdkRoot();
  std::string fingerprint = "clang=";
  fingerprint += CLANG_VERSION_STRING;
  fingerprint += "; resource-dir=";
  fingerprint += resourceDirectory ? resourceDirectory->string()
                                   : "unresolved: " + resourceDirectory.error();
  fingerprint += "; sdk=";
  fingerprint += sdkRoot ? sdkRoot->string() : std::string{};
  return fingerprint;
}

} // namespace facts
