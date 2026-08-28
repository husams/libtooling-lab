#include "tooling/CompilationCommandCodec.h"

#include <llvm/Support/Error.h>
#include <llvm/Support/JSON.h>

#include <algorithm>
#include <cstdlib>
#include <map>
#include <optional>
#include <ranges>
#include <regex>
#include <set>
#include <utility>

namespace facts {
namespace {

std::vector<long long> versionKey(std::string version) {
  if (version.starts_with('v')) {
    version.erase(0, 1);
  }
  std::vector<long long> key;
  const std::regex separator(R"([._-])");
  std::sregex_token_iterator part(version.begin(), version.end(), separator,
                                  -1);
  const std::sregex_token_iterator end;
  std::ranges::transform(
      part, end, std::back_inserter(key),
      [](const auto &value) { return std::stoll(value.str()); });
  return key;
}

struct VersionedRoot {
  std::filesystem::path base;
  std::filesystem::path root;
  std::vector<long long> version;
};

VersionedRoot splitVersion(std::filesystem::path root) {
  static const std::regex versionPattern(R"(^v?[0-9]+([._-][0-9]+)*$)");
  const auto segment = root.filename().string();
  if (!std::regex_match(segment, versionPattern) || root.parent_path() == "/") {
    return {root, root, {}};
  }
  return {root.parent_path(), root, versionKey(segment)};
}

std::optional<std::filesystem::path>
componentAliasRoot(const std::vector<std::filesystem::path> &roots) {
  auto candidates = roots | std::views::transform(splitVersion) |
                    std::ranges::to<std::vector>();
  if (!std::ranges::all_of(candidates, [&](const auto &candidate) {
        return candidate.base == candidates.front().base;
      })) {
    return std::nullopt;
  }
  return std::ranges::max(candidates, {}, &VersionedRoot::version).root;
}

StoredCommandAliases
componentAliases(const std::vector<StoredCompilationComponent> &components) {
  std::map<std::string, std::vector<std::filesystem::path>> grouped;
  for (const auto &component : components) {
    grouped[component.name].push_back(component.root);
  }
  StoredCommandAliases aliases;
  for (const auto &[name, roots] : grouped) {
    if (const auto root = componentAliasRoot(roots)) {
      aliases.insert_or_assign(name, root->string());
    }
  }
  return aliases;
}

StoredCommandAliases
mergeAliases(StoredCommandAliases labels,
             const std::vector<StoredCompilationComponent> &components) {
  for (const auto &[name, path] : componentAliases(components)) {
    labels.try_emplace(name, path);
  }
  return labels;
}

std::expected<std::vector<std::string>, std::string>
decodeOptions(std::string_view text) {
  auto parsed = llvm::json::parse(text);
  if (!parsed) {
    return std::unexpected(llvm::toString(parsed.takeError()));
  }
  const auto *array = parsed->getAsArray();
  if (!array) {
    return std::unexpected("compile_options is not a JSON array");
  }
  std::vector<std::string> options;
  options.reserve(array->size());
  for (const auto &value : *array) {
    const auto item = value.getAsString();
    if (!item) {
      return std::unexpected("compile_options contains a non-string value");
    }
    options.emplace_back(*item);
  }
  return options;
}

const std::set<std::string> drop = {"-c",
                                    "--",
                                    "-M",
                                    "-MM",
                                    "-MD",
                                    "-MMD",
                                    "-MG",
                                    "-MP",
                                    "-MV",
                                    "-Werror",
                                    "-pedantic-errors",
                                    "-shared",
                                    "-static",
                                    "-rdynamic",
                                    "-pie",
                                    "-no-pie",
                                    "-s",
                                    "-pipe",
                                    "-nostdlib",
                                    "-nodefaultlibs",
                                    "-nostartfiles",
                                    "-static-libgcc",
                                    "-shared-libgcc",
                                    "-static-libstdc++"};

const std::set<std::string> dropWithArgument = {"-o",
                                                "-MF",
                                                "-MT",
                                                "-MQ",
                                                "-dependency-file",
                                                "--serialize-diagnostics",
                                                "-Xlinker",
                                                "-T",
                                                "-L",
                                                "-l"};

constexpr std::string_view dropPrefixes[] = {"-Werror=",
                                             "-Wp,-M",
                                             "-MF",
                                             "-MT",
                                             "-MQ",
                                             "-l",
                                             "-L",
                                             "-Wl,",
                                             "-Wa,",
                                             "-fuse-ld=",
                                             "-fmodules-cache-path="};

bool hasDropPrefix(std::string_view option) {
  return std::ranges::any_of(dropPrefixes, [&](std::string_view prefix) {
    return option.starts_with(prefix);
  });
}

bool isEnvironmentAssignment(const std::string &option) {
  static const std::regex pattern(R"(^[A-Za-z_][A-Za-z0-9_-]*=)");
  return std::regex_search(option, pattern);
}

bool isLauncher(const std::string &option) {
  static const std::set<std::string> launchers = {
      "ccache", "sccache", "distcc", "icecc", "icerun", "env", "time", "nice"};
  return launchers.contains(std::filesystem::path(option).filename().string());
}

std::string expandLabels(std::string value,
                         const StoredCommandAliases &aliases) {
  std::size_t position = 0;
  while ((position = value.find('<', position)) != std::string::npos) {
    const auto close = value.find('>', position + 1);
    if (close == std::string::npos) {
      break;
    }
    const auto name = value.substr(position + 1, close - position - 1);
    auto replacement = aliases.find(name);
    std::string path;
    if (replacement != aliases.end()) {
      path = replacement->second;
    } else if (!name.empty()) {
      path = "/" + name;
      std::ranges::replace(path, '-', '/');
    } else {
      position = close + 1;
      continue;
    }
    value.replace(position, close - position + 1, path);
    position += path.size();
  }
  return value;
}

std::string expandEnvironment(std::string value) {
  static const std::regex variable(
      R"(\$\{([A-Za-z_][A-Za-z0-9_]*)\}|\$([A-Za-z_][A-Za-z0-9_]*))");
  std::smatch match;
  std::string result;
  while (std::regex_search(value, match, variable)) {
    result += match.prefix().str();
    const auto name = match[1].matched ? match[1].str() : match[2].str();
    if (const auto *replacement = std::getenv(name.c_str())) {
      result += replacement;
    } else {
      result += match.str();
    }
    value = match.suffix().str();
  }
  return result + value;
}

std::string resolvePath(std::string value,
                        const StoredCommandAliases &aliases) {
  const auto indirect = value.contains('<') || value.contains('$') ||
                        value == "~" || value.starts_with("~/");
  if (!indirect) {
    return value;
  }
  value = expandEnvironment(expandLabels(std::move(value), aliases));
  if (value == "~" || value.starts_with("~/")) {
    if (const auto *home = std::getenv("HOME")) {
      value.replace(0, 1, home);
    }
  }
  return normalizeCompilationPath(value).string();
}

std::vector<std::string>
resolveIncludePaths(std::vector<std::string> options,
                    const StoredCommandAliases &aliases) {
  constexpr std::string_view includeFlags[] = {
      "-I",           "-isystem",    "-iquote",   "-idirafter",
      "-F",           "-iframework", "-include",  "-imacros",
      "-include-pch", "--sysroot",   "-isysroot", "-resource-dir"};
  std::vector<std::string> result;
  for (std::size_t index = 0; index < options.size();) {
    auto option = std::move(options[index++]);
    const auto separate = std::ranges::find(includeFlags, option);
    if (separate != std::ranges::end(includeFlags)) {
      result.push_back(std::move(option));
      if (index < options.size()) {
        result.push_back(resolvePath(std::move(options[index++]), aliases));
      }
      continue;
    }
    const auto joined =
        std::ranges::find_if(includeFlags, [&](std::string_view flag) {
          return option.size() > flag.size() && option.starts_with(flag);
        });
    if (joined == std::ranges::end(includeFlags)) {
      result.push_back(std::move(option));
      continue;
    }
    result.push_back(std::string(*joined) +
                     resolvePath(option.substr(joined->size()), aliases));
  }
  return result;
}

void restoreSourceArgument(std::vector<std::string> &arguments,
                           const std::filesystem::path &source) {
  const auto marker = std::ranges::find(arguments, storedSourceArgumentMarker);
  if (marker == arguments.end()) {
    arguments.push_back(source.string());
    return;
  }
  std::ranges::replace(arguments, std::string(storedSourceArgumentMarker),
                       source.string());
}

clang::tooling::CompileCommand
assembleCompileCommand(const StoredCompileFile &file,
                       const StoredCommandAliases &aliases,
                       std::vector<std::string> arguments) {
  restoreSourceArgument(arguments, file.path);
  arguments.insert(arguments.begin(), file.driver.empty()
                                          ? defaultCompilerDriver(file.path)
                                          : file.driver);
  const auto directory =
      file.workingDirectory.empty()
          ? file.root
          : std::filesystem::path(resolvePath(file.workingDirectory, aliases));
  return {directory.string(), file.path.string(), std::move(arguments), ""};
}

std::string jsonQuote(std::string_view value) {
  std::string result;
  result.reserve(value.size() + 2);
  result.push_back('"');
  for (const unsigned char character : value) {
    switch (character) {
    case '"':
      result += "\\\"";
      break;
    case '\\':
      result += "\\\\";
      break;
    case '\n':
      result += "\\n";
      break;
    case '\r':
      result += "\\r";
      break;
    case '\t':
      result += "\\t";
      break;
    default:
      if (character < 0x20) {
        constexpr char hex[] = "0123456789abcdef";
        result += "\\u00";
        result.push_back(hex[character >> 4]);
        result.push_back(hex[character & 0x0f]);
      } else {
        result.push_back(static_cast<char>(character));
      }
      break;
    }
  }
  result.push_back('"');
  return result;
}

} // namespace

std::filesystem::path normalizeCompilationPath(std::filesystem::path path) {
  auto absolute =
      (path.is_absolute() ? std::move(path) : std::filesystem::absolute(path))
          .lexically_normal();
  std::error_code error;
  auto canonical = std::filesystem::weakly_canonical(absolute, error);
  return error ? absolute : canonical;
}

std::size_t commandStart(const std::vector<std::string> &arguments) {
  auto found = std::ranges::find_if(arguments, [](const auto &argument) {
    return !isEnvironmentAssignment(argument) && !isLauncher(argument);
  });
  return found == arguments.end() ? 0
                                  : static_cast<std::size_t>(std::distance(
                                        arguments.begin(), found));
}

std::vector<std::string>
sanitizeCommandArguments(std::vector<std::string> arguments) {
  std::vector<std::string> result;
  auto index =
      (!arguments.empty() && (isEnvironmentAssignment(arguments.front()) ||
                              isLauncher(arguments.front())))
          ? commandStart(arguments) + 1
          : std::size_t{0};
  while (index < arguments.size()) {
    auto argument = std::move(arguments[index++]);
    if (drop.contains(argument) || hasDropPrefix(argument)) {
      continue;
    }
    if (dropWithArgument.contains(argument)) {
      index = std::min(index + 1, arguments.size());
      continue;
    }
    result.push_back(std::move(argument));
  }
  return result;
}

std::string defaultCompilerDriver(const std::filesystem::path &source) {
  return source.extension() == ".c" ? "clang" : "clang++";
}

std::string encodeCompileOptions(const std::vector<std::string> &options) {
  std::string result = "[";
  for (std::size_t index = 0; index < options.size(); ++index) {
    if (index != 0) {
      result += ',';
    }
    result += jsonQuote(options[index]);
  }
  result += ']';
  return result;
}

std::expected<clang::tooling::CompileCommand, std::string>
decodeStoredCommand(const StoredCompileFile &file,
                    const StoredCommandAliases &aliases) {
  auto fileAliases = aliases;
  fileAliases.try_emplace(file.componentName, file.root.string());
  return decodeOptions(file.options)
      .transform(sanitizeCommandArguments)
      .transform([&](auto arguments) {
        return resolveIncludePaths(std::move(arguments), fileAliases);
      })
      .transform([&](auto arguments) {
        return assembleCompileCommand(file, fileAliases, std::move(arguments));
      });
}

std::expected<CompileCommands, std::string>
decodeCompileCommands(const StoredCompilationSnapshot &snapshot) {
  const auto aliases = mergeAliases(snapshot.labels, snapshot.components);
  CompileCommands commands;
  commands.reserve(snapshot.files.size());
  for (const auto &file : snapshot.files) {
    auto command = decodeStoredCommand(file, aliases);
    if (!command) {
      return std::unexpected(command.error());
    }
    commands.push_back(std::move(*command));
  }
  return commands;
}

} // namespace facts
