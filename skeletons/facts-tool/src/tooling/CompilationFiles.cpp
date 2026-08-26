#include "tooling/CompilationFiles.h"

#include <clang/Tooling/CompilationDatabase.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iterator>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <system_error>

namespace facts {
namespace {

using Paths = std::vector<std::filesystem::path>;

struct Discovery {
  std::vector<std::string> files;
  std::vector<std::string> diagnostics;
  // A source directory is a project tree and an include root is a header
  // search path. The two are walked under different rules, so they are kept
  // apart.
  Paths sourceRoots;
  Paths includeRoots;
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
  discovery.sourceRoots.push_back(
      std::filesystem::path(identity).parent_path());
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
        std::ranges::move(commandRoots,
                          std::back_inserter(discovery.includeRoots));
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

// Whether a suffix-less name may be a header. Only an include root spells one
// that way.
enum class SuffixlessNames { reject, admit };

// The walk sees everything that happens to live beside the headers: Python
// helpers, TableGen inputs, licences, build caches. None of that is a
// translation unit or a header extraction can ever resolve, so only C and C++
// inputs enter the registry. A suffix decides that for every named form,
// including the module units and CUDA sources a project may only reach through
// the walk.
bool compilableSuffix(std::string_view suffix) {
  static constexpr std::string_view suffixes[] = {
      ".c",   ".cc",   ".cp",  ".cpp", ".cxx", ".c++", ".m",    ".mm",
      ".h",   ".hh",   ".hp",  ".hpp", ".hxx", ".h++", ".def",  ".inc",
      ".inl", ".ipp",  ".tcc", ".tpp", ".cu",  ".cuh", ".ixx",  ".cppm",
      ".ccm", ".cxxm", ".mpp"};
  const auto lowered =
      suffix | std::views::transform([](unsigned char character) {
        return static_cast<char>(std::tolower(character));
      }) |
      std::ranges::to<std::string>();
  return std::ranges::contains(suffixes, lowered);
}

// A suffix-less file is how the C++ standard library and Qt spell a header
// (<string>, <__config>, <QString>), and also how a project spells README,
// LICENSE and Makefile. The first kind lives in a header search path and the
// second in a project tree, so only an include root admits a suffix-less name
// — and never one of the spellings that is documentation wherever it is found.
bool projectMetadata(std::string_view name) {
  static constexpr std::string_view names[] = {
      "AUTHORS",  "CHANGELOG", "CHANGES",     "CONTRIBUTING", "COPYING",
      "CREDITS",  "Dockerfile", "Doxyfile",   "GNUmakefile",  "INSTALL",
      "LICENCE",  "LICENSE",   "MANIFEST",    "Makefile",     "NEWS",
      "NOTICE",   "README",    "TODO",        "VERSION",      "makefile"};
  return std::ranges::contains(names, name);
}

bool compilableInput(const std::filesystem::path &path,
                     SuffixlessNames suffixless) {
  const auto name = path.filename().string();
  const auto suffix = path.extension().string();
  if (!suffix.empty()) {
    return compilableSuffix(suffix);
  }
  return suffixless == SuffixlessNames::admit && !name.empty() &&
         !name.starts_with('.') && !projectMetadata(name);
}

std::expected<void, std::string>
appendDirectoryEntries(const std::filesystem::path &root,
                       SuffixlessNames suffixless, Discovery &discovery) {
  std::error_code error;
  std::filesystem::recursive_directory_iterator entry(root, error);
  const std::filesystem::recursive_directory_iterator end;
  while (!error && entry != end) {
    if (compilableInput(entry->path(), suffixless) &&
        entry->is_regular_file(error)) {
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
appendDirectoryFiles(const std::filesystem::path &root,
                     SuffixlessNames suffixless, Discovery &discovery) {
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
  return directory ? appendDirectoryEntries(root, suffixless, discovery)
                   : std::expected<void, std::string>{};
}

void sortUniquePaths(Paths &paths) {
  std::ranges::sort(paths);
  paths.erase(std::ranges::unique(paths).begin(), paths.end());
}

std::expected<void, std::string> appendDiscoveredFiles(Discovery &discovery) {
  sortUniquePaths(discovery.sourceRoots);
  sortUniquePaths(discovery.includeRoots);
  // A directory named both ways is a header search path first: the wider rule
  // wins, and it is only walked once.
  const auto shared = std::ranges::remove_if(
      discovery.sourceRoots, [&](const std::filesystem::path &root) {
        return std::ranges::binary_search(discovery.includeRoots, root);
      });
  discovery.sourceRoots.erase(shared.begin(), shared.end());

  for (const auto &root : discovery.includeRoots) {
    auto appended =
        appendDirectoryFiles(root, SuffixlessNames::admit, discovery);
    if (!appended) {
      return std::unexpected(appended.error());
    }
  }
  for (const auto &root : discovery.sourceRoots) {
    auto appended =
        appendDirectoryFiles(root, SuffixlessNames::reject, discovery);
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
