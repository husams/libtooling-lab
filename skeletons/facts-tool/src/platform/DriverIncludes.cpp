#include "platform/DriverIncludes.h"

#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/FileUtilities.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Program.h"

#include <algorithm>
#include <array>
#include <map>
#include <mutex>
#include <ranges>
#include <sstream>
#include <string_view>

namespace facts::platform {
namespace {

using Arguments = std::vector<std::string>;
using IncludePaths = std::vector<std::filesystem::path>;

bool isGnuCxxDriver(const std::filesystem::path &driver) {
  const auto name = driver.filename().string();
  return name.find("g++") != std::string::npos &&
         name.find("clang++") == std::string::npos;
}

std::expected<std::filesystem::path, std::string>
resolveDriver(const clang::tooling::CompileCommand &command) {
  const std::filesystem::path driver(command.CommandLine.front());
  if (driver.has_parent_path()) {
    const auto resolved =
        driver.is_absolute()
            ? driver
            : std::filesystem::path(command.Directory) / driver;
    if (!std::filesystem::exists(resolved))
      return std::unexpected("GNU driver does not exist: " + resolved.string());
    return std::filesystem::weakly_canonical(resolved);
  }
  auto found = llvm::sys::findProgramByName(driver.string());
  if (!found)
    return std::unexpected("cannot resolve GNU driver '" + driver.string() +
                           "': " + found.getError().message());
  return std::filesystem::path(*found);
}

bool takesSeparateValue(std::string_view option) {
  constexpr std::array options{"--target",       "-target",   "--gcc-toolchain",
                               "-gcc-toolchain", "--sysroot", "-isysroot",
                               "-stdlib"};
  return std::ranges::find(options, option) != options.end();
}

bool isJoinedProbeOption(std::string_view option) {
  constexpr std::array prefixes{
      "--target=",       "-target=",   "--gcc-toolchain=",
      "-gcc-toolchain=", "--sysroot=", "-stdlib="};
  return option == "-m32" || option == "-m64" ||
         std::ranges::any_of(prefixes, [&](std::string_view prefix) {
           return option.starts_with(prefix);
         });
}

Arguments toolchainOptions(const Arguments &commandLine) {
  Arguments options;
  for (std::size_t index = 1; index < commandLine.size(); ++index) {
    const std::string_view option(commandLine[index]);
    if (takesSeparateValue(option) && index + 1 < commandLine.size()) {
      options.push_back(commandLine[index]);
      options.push_back(commandLine[++index]);
    } else if (isJoinedProbeOption(option)) {
      options.push_back(commandLine[index]);
    }
  }
  return options;
}

Arguments executableProbeOptions(const Arguments &options) {
  Arguments supported;
  for (std::size_t index = 0; index < options.size(); ++index) {
    const std::string_view option(options[index]);
    if ((option == "--sysroot" || option == "-isysroot") &&
        index + 1 < options.size()) {
      supported.push_back(options[index]);
      supported.push_back(options[++index]);
    } else if (option.starts_with("--sysroot=") || option == "-m32" ||
               option == "-m64") {
      supported.push_back(options[index]);
    }
  }
  return supported;
}

std::string cacheKey(const std::filesystem::path &driver,
                     const Arguments &options) {
  std::ostringstream key;
  key << driver.string();
  for (const auto &option : options)
    key << '\x1f' << option;
  return key.str();
}

std::expected<std::string, std::string>
runDriverProbe(const std::filesystem::path &driver, const Arguments &options) {
  llvm::SmallString<128> outputPath;
  llvm::SmallString<128> errorPath;
  int output = -1;
  int error = -1;
  if (auto ec = llvm::sys::fs::createTemporaryFile("facts-gxx-probe", "out",
                                                   output, outputPath))
    return std::unexpected("cannot create GNU driver probe output: " +
                           ec.message());
  llvm::sys::fs::closeFile(output);
  llvm::FileRemover removeOutput(outputPath);
  if (auto ec = llvm::sys::fs::createTemporaryFile("facts-gxx-probe", "err",
                                                   error, errorPath))
    return std::unexpected("cannot create GNU driver probe diagnostics: " +
                           ec.message());
  llvm::sys::fs::closeFile(error);
  llvm::FileRemover removeError(errorPath);

  Arguments owned{driver.string()};
  owned.insert(owned.end(), options.begin(), options.end());
  owned.insert(owned.end(), {"-E", "-x", "c++", "-", "-v"});
  llvm::SmallVector<llvm::StringRef> arguments;
  std::ranges::transform(
      owned, std::back_inserter(arguments),
      [](const std::string &value) { return llvm::StringRef(value); });
  const std::array<std::optional<llvm::StringRef>, 3> redirects{
      llvm::StringRef("/dev/null"), llvm::StringRef(outputPath),
      llvm::StringRef(errorPath)};
  std::string executionError;
  bool executionFailed = false;
  const int result = llvm::sys::ExecuteAndWait(
      driver.string(), arguments, std::nullopt, redirects, 0, 0,
      &executionError, &executionFailed);
  auto diagnostics = llvm::MemoryBuffer::getFile(errorPath);
  const std::string text =
      diagnostics ? (*diagnostics)->getBuffer().str() : std::string{};
  if (executionFailed || result != 0) {
    std::ostringstream message;
    message << "GNU driver probe failed for " << driver.string() << " (exit "
            << result << ")";
    if (!executionError.empty())
      message << ": " << executionError;
    if (!text.empty())
      message << "\n" << text;
    return std::unexpected(message.str());
  }
  return text;
}

bool isCxxLibraryDirectory(const std::filesystem::path &path) {
  const auto value = path.generic_string();
  return value.find("/include/c++/") != std::string::npos ||
         value.ends_with("/include/c++");
}

std::expected<IncludePaths, std::string>
parseIncludeSearch(const std::filesystem::path &driver,
                   const std::string &diagnostics) {
  std::istringstream lines(diagnostics);
  std::string line;
  bool collecting = false;
  IncludePaths includes;
  while (std::getline(lines, line)) {
    if (line.find("#include <...> search starts here:") != std::string::npos) {
      collecting = true;
      continue;
    }
    if (collecting && line.find("End of search list.") != std::string::npos)
      break;
    if (!collecting)
      continue;
    const auto first = line.find_first_not_of(" \t");
    if (first == std::string::npos)
      continue;
    auto value = line.substr(first);
    constexpr std::string_view framework = " (framework directory)";
    if (value.ends_with(framework))
      value.erase(value.size() - framework.size());
    std::filesystem::path include(value);
    if (isCxxLibraryDirectory(include))
      includes.push_back(std::move(include));
  }
  if (includes.empty())
    return std::unexpected(
        "GNU driver probe produced no C++ include paths for " +
        driver.string() + "\n" + diagnostics);
  return includes;
}

std::expected<IncludePaths, std::string>
discoverIncludes(const clang::tooling::CompileCommand &command) {
  return resolveDriver(command).and_then([&](const auto &driver) {
    const auto options = toolchainOptions(command.CommandLine);
    const auto key = cacheKey(driver, options);
    static std::mutex mutex;
    static std::map<std::string, std::expected<IncludePaths, std::string>>
        cache;
    {
      const std::lock_guard lock(mutex);
      if (const auto found = cache.find(key); found != cache.end())
        return found->second;
    }
    auto discovered = runDriverProbe(driver, executableProbeOptions(options))
                          .and_then([&](const auto &diagnostics) {
                            return parseIncludeSearch(driver, diagnostics);
                          });
    const std::lock_guard lock(mutex);
    return cache.emplace(key, discovered).first->second;
  });
}

void removeResourceDirectory(Arguments &arguments) {
  for (std::size_t index = 1; index < arguments.size();) {
    if (arguments[index] == "-resource-dir") {
      const auto count = index + 1 < arguments.size() ? 2U : 1U;
      arguments.erase(arguments.begin() + index,
                      arguments.begin() + index + count);
    } else if (arguments[index].starts_with("-resource-dir=")) {
      arguments.erase(arguments.begin() + index);
    } else {
      ++index;
    }
  }
}

bool hasSysroot(const Arguments &arguments) {
  return std::ranges::any_of(arguments, [](const std::string &argument) {
    return argument == "-isysroot" || argument == "--sysroot" ||
           argument.starts_with("--sysroot=") ||
           argument.starts_with("-isysroot=");
  });
}

std::vector<std::string> explicitIncludePaths(const Arguments &arguments) {
  std::vector<std::string> includes;
  for (std::size_t index = 1; index < arguments.size(); ++index) {
    if ((arguments[index] == "-isystem" || arguments[index] == "-I") &&
        index + 1 < arguments.size()) {
      includes.push_back(arguments[++index]);
    } else if (arguments[index].starts_with("-isystem") &&
               arguments[index].size() > 8) {
      includes.push_back(arguments[index].substr(8));
    } else if (arguments[index].starts_with("-I") &&
               arguments[index].size() > 2) {
      includes.push_back(arguments[index].substr(2));
    }
  }
  return includes;
}

clang::tooling::CompileCommand
appendIncludes(clang::tooling::CompileCommand command,
               const IncludePaths &includes) {
  auto existing = explicitIncludePaths(command.CommandLine);
  for (const auto &include : includes) {
    const auto value = include.string();
    if (std::ranges::find(existing, value) == existing.end()) {
      command.CommandLine.insert(command.CommandLine.end(),
                                 {"-isystem", value});
      existing.push_back(value);
    }
  }
  return command;
}

} // namespace

std::expected<clang::tooling::CompileCommand, std::string>
configureCommand(clang::tooling::CompileCommand command,
                 const std::filesystem::path &resourceDirectory,
                 const std::optional<std::filesystem::path> &sdkRoot) {
  if (command.CommandLine.empty())
    return std::unexpected("compile command has no target driver: " +
                           command.Filename);
  removeResourceDirectory(command.CommandLine);
  command.CommandLine.insert(command.CommandLine.end(),
                             {"-resource-dir", resourceDirectory.string()});
  if (sdkRoot && !hasSysroot(command.CommandLine))
    command.CommandLine.insert(command.CommandLine.end(),
                               {"-isysroot", sdkRoot->string()});
  if (!isGnuCxxDriver(command.CommandLine.front()))
    return command;
  return discoverIncludes(command).transform(
      [&](const auto &includes) { return appendIncludes(command, includes); });
}

} // namespace facts::platform
