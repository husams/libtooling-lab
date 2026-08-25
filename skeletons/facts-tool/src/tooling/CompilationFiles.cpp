#include "tooling/CompilationFiles.h"

#include <clang/Tooling/CompilationDatabase.h>

#include <algorithm>
#include <filesystem>
#include <iterator>
#include <optional>
#include <ranges>
#include <string_view>
#include <system_error>

namespace facts {
namespace {

using Paths = std::vector<std::filesystem::path>;

struct Discovery {
  std::vector<std::string> files;
  std::vector<std::string> diagnostics;
  Paths roots;
};

std::string describe(std::string_view action, const std::filesystem::path &path,
                     const std::error_code &error) {
  return std::string(action) + " '" + path.string() + "': " + error.message();
}

bool missing(const std::error_code &error) {
  return error == std::errc::no_such_file_or_directory ||
         error == std::errc::not_a_directory;
}

std::expected<std::string, std::string>
requireSourceIdentity(const std::filesystem::path &path) {
  std::error_code error;
  auto identity = std::filesystem::canonical(path, error);
  if (error) {
    return std::unexpected(describe("cannot resolve source file", path, error));
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

void appendSourceIdentity(std::string identity, Discovery &discovery) {
  discovery.roots.push_back(std::filesystem::path(identity).parent_path());
  discovery.files.push_back(std::move(identity));
}

std::expected<void, std::string>
appendCommand(const clang::tooling::CompileCommand &command,
              Discovery &discovery) {
  return requireSourceIdentity(resolve(std::filesystem::path(command.Directory),
                                       command.Filename))
      .transform([&](std::string identity) {
        appendSourceIdentity(std::move(identity), discovery);
        auto commandRoots = includeRoots(command);
        std::ranges::move(commandRoots, std::back_inserter(discovery.roots));
      });
}

std::expected<void, std::string>
appendSelectedSource(const clang::tooling::CompilationDatabase &compilations,
                     const std::string &source, Discovery &discovery) {
  auto identity = requireSourceIdentity(source);
  if (!identity) {
    return std::unexpected(identity.error());
  }
  appendSourceIdentity(std::move(*identity), discovery);
  for (const auto &command : compilations.getCompileCommands(source)) {
    auto appended = appendCommand(command, discovery);
    if (!appended) {
      return std::unexpected(appended.error());
    }
  }
  return {};
}

std::expected<void, std::string>
appendCommandSources(const clang::tooling::CompilationDatabase &compilations,
                     std::span<const std::string> selectedSources,
                     Discovery &discovery) {
  if (selectedSources.empty()) {
    for (const auto &command : compilations.getAllCompileCommands()) {
      auto appended = appendCommand(command, discovery);
      if (!appended) {
        return std::unexpected(appended.error());
      }
    }
    return {};
  }
  for (const auto &source : selectedSources) {
    auto appended = appendSelectedSource(compilations, source, discovery);
    if (!appended) {
      return std::unexpected(appended.error());
    }
  }
  return {};
}

std::expected<void, std::string>
appendDirectoryEntries(const std::filesystem::path &root,
                       Discovery &discovery) {
  std::error_code error;
  std::filesystem::recursive_directory_iterator entry(root, error);
  const std::filesystem::recursive_directory_iterator end;
  while (!error && entry != end) {
    if (entry->is_regular_file(error)) {
      std::error_code identityError;
      auto identity = std::filesystem::canonical(entry->path(), identityError);
      if (identityError && !missing(identityError)) {
        return std::unexpected(
            describe("cannot resolve file", entry->path(), identityError));
      }
      if (!identityError) {
        discovery.files.push_back(identity.lexically_normal().string());
      }
    }
    entry.increment(error);
  }
  return error ? std::unexpected(
                     describe("cannot scan include directory", root, error))
               : std::expected<void, std::string>{};
}

std::expected<void, std::string>
appendDirectoryFiles(const std::filesystem::path &root, Discovery &discovery) {
  std::error_code error;
  const bool directory = std::filesystem::is_directory(root, error);
  if (missing(error)) {
    discovery.diagnostics.push_back(
        describe("skipping unavailable include directory", root, error));
    return {};
  }
  if (error) {
    return std::unexpected(
        describe("cannot inspect include directory", root, error));
  }
  return directory ? appendDirectoryEntries(root, discovery)
                   : std::expected<void, std::string>{};
}

std::expected<void, std::string> appendDiscoveredFiles(Discovery &discovery) {
  std::ranges::sort(discovery.roots);
  discovery.roots.erase(std::ranges::unique(discovery.roots).begin(),
                        discovery.roots.end());
  for (const auto &root : discovery.roots) {
    auto appended = appendDirectoryFiles(root, discovery);
    if (!appended) {
      return std::unexpected(appended.error());
    }
  }
  return {};
}

CompilationFiles finalize(Discovery discovery) {
  std::ranges::sort(discovery.files);
  discovery.files.erase(std::ranges::unique(discovery.files).begin(),
                        discovery.files.end());
  return {std::move(discovery.files), std::move(discovery.diagnostics)};
}

} // namespace

std::expected<CompilationFiles, std::string> discoverCompilationFiles(
    const clang::tooling::CompilationDatabase &compilations,
    std::span<const std::string> selectedSources) {
  Discovery discovery;
  return appendCommandSources(compilations, selectedSources, discovery)
      .and_then([&] { return appendDiscoveredFiles(discovery); })
      .transform([&] { return finalize(std::move(discovery)); });
}

} // namespace facts
