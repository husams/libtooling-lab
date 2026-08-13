#include "tooling/CompilationFiles.h"

#include <clang/Tooling/CompilationDatabase.h>

#include <algorithm>
#include <filesystem>
#include <iterator>
#include <optional>
#include <ranges>
#include <string_view>

namespace facts {
namespace {

using Paths = std::vector<std::filesystem::path>;

std::expected<std::string, std::error_code>
canonicalIdentity(const std::filesystem::path &path) {
  std::error_code error;
  auto identity = std::filesystem::canonical(path, error);
  if (error) {
    return std::unexpected(error);
  }
  return identity.lexically_normal().string();
}

std::filesystem::path resolve(const std::filesystem::path &directory,
                              std::string_view value) {
  std::filesystem::path path(value);
  return path.is_absolute() ? path : directory / path;
}

std::optional<std::string_view> joinedValue(std::string_view argument,
                                            std::string_view option) {
  if (!argument.starts_with(option) || argument.size() == option.size()) {
    return std::nullopt;
  }
  auto value = argument.substr(option.size());
  return value.starts_with('=') ? value.substr(1) : value;
}

bool separateValueOption(std::string_view argument) {
  return argument == "-I" || argument == "-isystem" || argument == "-iquote" ||
         argument == "-idirafter" || argument == "-F" ||
         argument == "-iframework" || argument == "/I";
}

std::optional<std::string_view> joinedIncludeValue(std::string_view argument) {
  constexpr std::string_view options[] = {
      "-isystem", "-iquote", "-idirafter", "-iframework", "-I", "-F", "/I"};
  auto matched = std::ranges::find_if(options, [&](std::string_view option) {
    return joinedValue(argument, option).has_value();
  });
  return matched == std::ranges::end(options) ? std::nullopt
                                              : joinedValue(argument, *matched);
}

std::optional<std::filesystem::path>
includeRootAt(const clang::tooling::CompileCommand &command,
              std::size_t index) {
  const std::string_view argument = command.CommandLine[index];
  const std::filesystem::path directory(command.Directory);
  if (separateValueOption(argument) && index + 1 < command.CommandLine.size()) {
    return resolve(directory, command.CommandLine[index + 1]);
  }
  return joinedIncludeValue(argument).transform(
      [&](std::string_view value) { return resolve(directory, value); });
}

Paths includeRoots(const clang::tooling::CompileCommand &command) {
  auto roots =
      std::views::iota(std::size_t{1}, command.CommandLine.size()) |
      std::views::transform(
          [&](std::size_t index) { return includeRootAt(command, index); }) |
      std::views::filter([](const auto &root) { return root.has_value(); }) |
      std::views::transform([](auto root) { return std::move(*root); });
  return roots | std::ranges::to<Paths>();
}

std::expected<void, std::error_code>
appendCommand(const clang::tooling::CompileCommand &command,
              std::vector<std::string> &files, Paths &roots) {
  auto identity = canonicalIdentity(
      resolve(std::filesystem::path(command.Directory), command.Filename));
  if (!identity) {
    return std::unexpected(identity.error());
  }
  files.push_back(*identity);
  roots.push_back(std::filesystem::path(*identity).parent_path());
  auto commandRoots = includeRoots(command);
  std::ranges::move(commandRoots, std::back_inserter(roots));
  return {};
}

std::expected<void, std::error_code>
appendDirectoryFiles(const std::filesystem::path &root,
                     std::vector<std::string> &files) {
  std::error_code error;
  if (!std::filesystem::is_directory(root, error)) {
    return error ? std::unexpected(error)
                 : std::expected<void, std::error_code>{};
  }

  constexpr auto options =
      std::filesystem::directory_options::skip_permission_denied;
  std::filesystem::recursive_directory_iterator entry(root, options, error);
  const std::filesystem::recursive_directory_iterator end;
  while (!error && entry != end) {
    if (entry->is_regular_file(error)) {
      auto identity = canonicalIdentity(entry->path());
      if (!identity) {
        return std::unexpected(identity.error());
      }
      files.push_back(std::move(*identity));
    }
    entry.increment(error);
  }
  return error ? std::unexpected(error)
               : std::expected<void, std::error_code>{};
}

std::expected<void, std::error_code>
appendFallbackSource(std::string_view source, std::vector<std::string> &files,
                     Paths &roots) {
  auto identity = canonicalIdentity(source);
  if (!identity) {
    return std::unexpected(identity.error());
  }
  files.push_back(*identity);
  roots.push_back(std::filesystem::path(*identity).parent_path());
  return {};
}

} // namespace

std::expected<std::vector<std::string>, std::error_code>
discoverCompilationFiles(
    const clang::tooling::CompilationDatabase &compilations,
    std::span<const std::string> fallbackSources) {
  std::vector<std::string> files;
  Paths roots;
  const auto commands = compilations.getAllCompileCommands();
  if (commands.empty()) {
    for (const auto &source : fallbackSources) {
      auto appended = appendFallbackSource(source, files, roots);
      if (!appended) {
        return std::unexpected(appended.error());
      }
      for (const auto &command : compilations.getCompileCommands(source)) {
        appended = appendCommand(command, files, roots);
        if (!appended) {
          return std::unexpected(appended.error());
        }
      }
    }
  } else {
    for (const auto &command : commands) {
      auto appended = appendCommand(command, files, roots);
      if (!appended) {
        return std::unexpected(appended.error());
      }
    }
  }

  std::ranges::sort(roots);
  roots.erase(std::ranges::unique(roots).begin(), roots.end());
  for (const auto &root : roots) {
    auto appended = appendDirectoryFiles(root, files);
    if (!appended) {
      return std::unexpected(appended.error());
    }
  }
  std::ranges::sort(files);
  files.erase(std::ranges::unique(files).begin(), files.end());
  return files;
}

} // namespace facts
