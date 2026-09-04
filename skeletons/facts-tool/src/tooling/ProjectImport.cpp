#include "tooling/ProjectImport.h"

#include "storage/FileManager.h"
#include "tooling/CompilationCommandCodec.h"

#include <algorithm>
#include <filesystem>
#include <map>
#include <optional>
#include <ranges>
#include <sstream>
#include <utility>

namespace facts {
namespace {

bool ownsPath(const std::filesystem::path &root,
              const std::filesystem::path &path) {
  auto key = root.lexically_normal().string();
  while (!key.empty() && key.back() == '/') {
    key.pop_back();
  }
  const auto identity = path.lexically_normal().string();
  return identity == key || identity.starts_with(key + "/");
}

std::string portablePath(const std::filesystem::path &path,
                         const ProjectComponent &component,
                         const ProjectClone &clone) {
  const auto root = effectiveComponentRoot(component, clone);
  if (!ownsPath(root, path)) {
    return path.lexically_normal().string();
  }
  const auto relative = path.lexically_relative(root);
  return relative.empty() || relative == "."
             ? "<" + component.name + ">"
             : "<" + component.name + ">/" + relative.generic_string();
}

std::filesystem::path
resolveCommandPath(const clang::tooling::CompileCommand &command,
                   std::string_view value) {
  const std::filesystem::path path(value);
  return normalizeCompilationPath(
      path.is_absolute() ? path
                         : std::filesystem::path(command.Directory) / path);
}

std::filesystem::path
logicalCommandPath(const clang::tooling::CompileCommand &command,
                   std::string_view value) {
  const std::filesystem::path path(value);
  return logicalCompilationPath(
      path.is_absolute() ? path
                         : std::filesystem::path(command.Directory) / path);
}

struct ProjectRoot {
  std::filesystem::path logical;
  std::filesystem::path canonical;
};

std::filesystem::path projectPath(const ProjectRoot &root,
                                  const clang::tooling::CompileCommand &command,
                                  std::string_view value) {
  const auto logical = logicalCommandPath(command, value);
  if (!ownsPath(root.logical, logical)) {
    return resolveCommandPath(command, value);
  }
  const auto relative = logical.lexically_relative(root.logical);
  return relative.empty() || relative == "."
             ? root.canonical
             : (root.canonical / relative).lexically_normal();
}

std::string normalizePathValue(const ProjectRoot &root,
                               const clang::tooling::CompileCommand &command,
                               std::string value,
                               const ProjectComponent &component,
                               const ProjectClone &clone) {
  return portablePath(projectPath(root, command, value), component, clone);
}

std::string compilerDriver(const clang::tooling::CompileCommand &command,
                           std::size_t start,
                           const std::filesystem::path &source) {
  const auto candidate = command.CommandLine[start];
  const auto name = std::filesystem::path(candidate).filename().string();
  const auto gnuCxx = name == "g++" || name.starts_with("g++-") ||
                      name.ends_with("-g++") ||
                      name.find("-g++-") != std::string::npos;
  const auto compilerName =
      name == "clang" || name.starts_with("clang++") ||
      (name.starts_with("clang-") && name != "clang-tool") || name == "gcc" ||
      name.starts_with("gcc-") || name == "cc" || name == "c++" || gnuCxx;
  const auto usablePath =
      !candidate.contains('/') || std::filesystem::exists(candidate);
  return compilerName && usablePath ? candidate : defaultCompilerDriver(source);
}

std::vector<std::string> normalizePathOptions(
    const ProjectRoot &root, const clang::tooling::CompileCommand &command,
    std::vector<std::string> options, const ProjectComponent &component,
    const ProjectClone &clone) {
  constexpr std::string_view pathFlags[] = {
      "-include-pch", "-iframework", "-idirafter", "-resource-dir",
      "--sysroot",    "-isystem",    "-iquote",    "-isysroot",
      "-F",           "-I"};
  const auto pathFlag = [&](std::string_view option) {
    return std::ranges::find_if(pathFlags, [&](std::string_view flag) {
      return option == flag ||
             (option.starts_with(flag) && option.size() > flag.size() &&
              (flag.starts_with("--") ? option[flag.size()] == '='
                                      : option[flag.size()] != '='));
    });
  };

  std::vector<std::string> result;
  for (std::size_t index = 0; index < options.size();) {
    auto option = std::move(options[index++]);
    const auto matched = pathFlag(option);
    if (matched == std::ranges::end(pathFlags)) {
      result.push_back(std::move(option));
      continue;
    }
    const auto flag = *matched;
    if (option == flag) {
      result.push_back(std::move(option));
      if (index < options.size()) {
        result.push_back(normalizePathValue(
            root, command, std::move(options[index++]), component, clone));
      }
      continue;
    }
    auto value = option.substr(flag.size());
    if (value.starts_with('=')) {
      value.erase(0, 1);
    }
    result.push_back(
        std::string(flag) + (flag.starts_with("--") ? "=" : "") +
        normalizePathValue(root, command, std::move(value), component, clone));
  }
  return result;
}

using PathSpelling = std::filesystem::path (*)(
    const clang::tooling::CompileCommand &, std::string_view);

std::filesystem::path
canonicalSpelling(const clang::tooling::CompileCommand &command,
                  std::string_view value) {
  return resolveCommandPath(command, value);
}

std::filesystem::path
logicalSpelling(const clang::tooling::CompileCommand &command,
                std::string_view value) {
  return logicalCommandPath(command, value);
}

std::filesystem::path commonRoot(const CompileCommands &commands,
                                 PathSpelling spelling) {
  auto root = spelling(commands.front(), commands.front().Directory);
  const auto lift = [&root](const std::filesystem::path &path) {
    while (!ownsPath(root, path)) {
      const auto parent = root.parent_path();
      if (parent == root) {
        return false;
      }
      root = parent;
    }
    return true;
  };
  for (const auto &command : commands) {
    if (!lift(spelling(command, command.Directory)) ||
        !lift(spelling(command, command.Filename))) {
      return root;
    }
  }
  return root;
}

std::size_t depth(const std::filesystem::path &path) {
  return static_cast<std::size_t>(std::ranges::distance(path));
}

std::optional<std::filesystem::path> gitRoot(std::filesystem::path directory) {
  auto current = logicalCompilationPath(std::move(directory));
  while (true) {
    std::error_code error;
    const auto dotGit = current / ".git";
    if (std::filesystem::is_directory(dotGit, error) ||
        std::filesystem::is_regular_file(dotGit, error)) {
      return current;
    }
    const auto parent = current.parent_path();
    if (parent == current) {
      return std::nullopt;
    }
    current = parent;
  }
}

ProjectRoot selectProjectRoot(const CompileCommands &commands) {
  const auto anchor = [](std::filesystem::path root) {
    return ProjectRoot{root, normalizeCompilationPath(root)};
  };
  const auto logical = commonRoot(commands, logicalSpelling);
  const auto canonical = commonRoot(commands, canonicalSpelling);
  if (auto repository = gitRoot(logical)) {
    return anchor(std::move(*repository));
  }
  if (auto repository = gitRoot(canonical)) {
    return anchor(std::move(*repository));
  }
  return anchor(depth(canonical) > depth(logical) ? canonical : logical);
}

std::string repositoryName(const std::filesystem::path &cloneRoot) {
  auto name = cloneRoot.filename().string();
  return name.empty() ? cloneRoot.parent_path().filename().string() : name;
}

ProjectComponent normalizeComponent(ProjectComponent component,
                                    const ProjectClone &clone) {
  const auto path = std::filesystem::path(component.path);
  const auto resolved = normalizeCompilationPath(
      path.is_absolute() ? path : std::filesystem::path(clone.path) / path);
  const auto cloneRoot = normalizeCompilationPath(clone.path);
  if (ownsPath(cloneRoot, resolved)) {
    const auto relative = resolved.lexically_relative(cloneRoot);
    component.path = relative.empty() ? "." : relative.generic_string();
  } else {
    component.path = resolved.generic_string();
  }
  return component;
}

struct PreparedCommand {
  std::filesystem::path source;
  std::size_t component = 0;
  std::string key;
  std::string driver;
  std::string workingDirectory;
  std::vector<std::string> options;
};

bool isSourceArgument(const ProjectRoot &root,
                      const clang::tooling::CompileCommand &command,
                      const std::filesystem::path &source,
                      const std::string &argument) {
  return argument == command.Filename || argument == source.string() ||
         (!argument.starts_with('-') &&
          projectPath(root, command, argument) == source);
}

std::vector<std::string> preserveSourcePosition(
    const ProjectRoot &root, const clang::tooling::CompileCommand &command,
    const std::filesystem::path &source, std::vector<std::string> arguments) {
  std::vector<std::string> preserved;
  preserved.reserve(arguments.size());
  bool sourceRecorded = false;
  for (auto &argument : arguments) {
    if (isSourceArgument(root, command, source, argument)) {
      if (!std::exchange(sourceRecorded, true)) {
        preserved.emplace_back(storedSourceArgumentMarker);
      }
      continue;
    }
    preserved.push_back(std::move(argument));
  }
  return preserved;
}

std::expected<PreparedCommand, std::string> prepareCommand(
    const ProjectRoot &root, const clang::tooling::CompileCommand &command,
    std::span<const ProjectComponent> components, const ProjectClone &clone) {
  if (command.CommandLine.empty()) {
    return std::unexpected("compile command has no arguments");
  }
  const auto source = projectPath(root, command, command.Filename);
  const auto component = selectOwningComponent(components, clone, source);
  if (!component) {
    return std::unexpected("source is outside every configured component: " +
                           source.string());
  }
  const auto start = commandStart(command.CommandLine);
  if (start >= command.CommandLine.size()) {
    return std::unexpected("compile command has no compiler driver: " +
                           source.string());
  }
  std::vector<std::string> options(command.CommandLine.begin() + start + 1,
                                   command.CommandLine.end());
  options = preserveSourcePosition(root, command, source, std::move(options));
  options = sanitizeCommandArguments(std::move(options));
  options = normalizePathOptions(root, command, std::move(options),
                                 components[*component], clone);
  const auto componentRoot =
      effectiveComponentRoot(components[*component], clone);
  const auto directory = source.lexically_relative(componentRoot).parent_path();
  const auto workingDirectory =
      portablePath(projectPath(root, command, command.Directory),
                   components[*component], clone);
  std::ostringstream key;
  key << source.string() << '\x1f' << command.Directory << '\x1f'
      << command.CommandLine[start];
  for (const auto &option : options) {
    key << '\x1f' << option;
  }
  return PreparedCommand{
      source,           *component,
      key.str(),        compilerDriver(command, start, source),
      workingDirectory, std::move(options)};
}

CompileCommands
selectCommands(const clang::tooling::CompilationDatabase &database,
               std::span<const std::string> requestedSources) {
  if (requestedSources.empty()) {
    return database.getAllCompileCommands();
  }
  CompileCommands commands;
  for (const auto &source : requestedSources) {
    auto selected = database.getCompileCommands(source);
    commands.insert(commands.end(), selected.begin(), selected.end());
  }
  return commands;
}

struct PreparedImport {
  ProjectConfiguration configuration;
  ProjectImportResult result;
};

std::expected<PreparedImport, std::string>
prepareCommands(const clang::tooling::CompilationDatabase &database,
                std::span<const std::string> requestedSources,
                const ProjectImportOptions &options) {
  auto commands = selectCommands(database, requestedSources);
  if (commands.empty()) {
    return std::unexpected("compilation database contains no commands");
  }

  ProjectConfiguration configuration;
  const auto root = selectProjectRoot(commands);
  configuration.activeClone.path = root.canonical.string();
  configuration.repositoryName =
      options.repositoryName.empty()
          ? repositoryName(configuration.activeClone.path)
          : options.repositoryName;
  configuration.remoteUrl = options.remoteUrl;
  configuration.activeClone.label = options.cloneLabel;
  configuration.components = options.components;
  if (configuration.components.empty()) {
    configuration.components.push_back(ProjectComponent{
        .name = configuration.repositoryName, .path = ".", .kind = "repo"});
  }
  for (auto &component : configuration.components) {
    component =
        normalizeComponent(std::move(component), configuration.activeClone);
    component.repositoryId = 1;
  }

  std::map<std::string, PreparedCommand> selected;
  ProjectImportResult result;
  for (const auto &command : commands) {
    auto prepared = prepareCommand(root, command, configuration.components,
                                   configuration.activeClone);
    if (!prepared) {
      return std::unexpected(prepared.error());
    }
    auto candidate = std::move(*prepared);
    const auto source = candidate.source.string();
    const auto entry = selected.find(source);
    if (entry == selected.end()) {
      selected.emplace(source, std::move(candidate));
      continue;
    }
    ++result.duplicateCommands;
    result.diagnostics.push_back("duplicate compile command for " + source +
                                 "; selected the deterministic winner");
    if (entry->second.key > candidate.key) {
      entry->second = std::move(candidate);
    }
  }

  for (const auto &[source, command] : selected) {
    const auto &component = configuration.components[command.component];
    const auto relative = command.source.lexically_relative(
        effectiveComponentRoot(component, configuration.activeClone));
    configuration.files.push_back(
        ProjectFile{.componentPath = component.path,
                    .directory = relative.parent_path() == "."
                                     ? std::string{}
                                     : relative.parent_path().generic_string(),
                    .name = relative.filename().generic_string(),
                    .driver = command.driver,
                    .workingDirectory = command.workingDirectory,
                    .compileOptions = encodeCompileOptions(command.options)});
  }
  result.importedFiles = configuration.files.size();
  return PreparedImport{std::move(configuration), std::move(result)};
}

std::expected<PreparedImport, std::string>
validatePreparedCommands(PreparedImport prepared) {
  if (auto validation = validateProjectConfiguration(prepared.configuration);
      !validation) {
    return std::unexpected("cannot store project configuration: " +
                           validation.error());
  }
  return prepared;
}

std::expected<ProjectImportResult, std::string>
persistPreparedCommands(FileManager &files, PreparedImport prepared) {
  if (auto imported = files.replaceProjectConfiguration(prepared.configuration);
      !imported) {
    return std::unexpected("cannot store project configuration: " +
                           imported.error());
  }
  return std::move(prepared.result);
}

} // namespace

std::expected<ProjectImportResult, std::string>
importProjectConfiguration(FileManager &files,
                           const clang::tooling::CompilationDatabase &database,
                           std::span<const std::string> requestedSources,
                           const ProjectImportOptions &options) {
  return prepareCommands(database, requestedSources, options)
      .and_then(validatePreparedCommands)
      .and_then([&files](PreparedImport prepared) {
        return persistPreparedCommands(files, std::move(prepared));
      });
}

} // namespace facts
