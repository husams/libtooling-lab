#include "tooling/StoredCompilationDatabase.h"

#include "storage/FileManager.h"

#include <llvm/Support/Error.h>
#include <llvm/Support/JSON.h>

#include <sqlite3.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <map>
#include <ranges>
#include <regex>
#include <set>
#include <sstream>
#include <string_view>
#include <utility>
#include <vector>

namespace facts {
namespace {

using Statement = std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)>;
using Commands = std::vector<clang::tooling::CompileCommand>;
using Aliases = std::map<std::string, std::string>;

struct Connection {
  sqlite3 *handle = nullptr;

  Connection() = default;
  Connection(const Connection &) = delete;
  Connection &operator=(const Connection &) = delete;

  Connection(Connection &&other) noexcept
      : handle(std::exchange(other.handle, nullptr)) {}

  Connection &operator=(Connection &&other) noexcept {
    if (handle) {
      sqlite3_close(handle);
    }
    handle = std::exchange(other.handle, nullptr);
    return *this;
  }

  ~Connection() {
    if (handle) {
      sqlite3_close(handle);
    }
  }
};

struct Component {
  std::int64_t id;
  std::string name;
  std::filesystem::path root;
};

struct StoredCompileFile {
  std::filesystem::path root;
  std::filesystem::path path;
  std::string componentName;
  std::string driver;
  std::string workingDirectory;
  std::string options;
};

std::string sqliteError(sqlite3 *database) { return sqlite3_errmsg(database); }

std::expected<Connection, std::string>
openReadOnly(const std::filesystem::path &path) {
  if (!std::filesystem::is_regular_file(path)) {
    return std::unexpected("stored compilation database does not exist: " +
                           path.string());
  }
  Connection connection;
  if (sqlite3_open_v2(path.c_str(), &connection.handle, SQLITE_OPEN_READONLY,
                      nullptr) != SQLITE_OK) {
    return std::unexpected(sqliteError(connection.handle));
  }
  return connection;
}

std::expected<Statement, std::string> prepare(sqlite3 *database,
                                              std::string_view sql) {
  sqlite3_stmt *raw = nullptr;
  if (sqlite3_prepare_v2(database, sql.data(), static_cast<int>(sql.size()),
                         &raw, nullptr) != SQLITE_OK) {
    return std::unexpected(sqliteError(database));
  }
  return Statement(raw, sqlite3_finalize);
}

std::string columnText(sqlite3_stmt *statement, int column) {
  const auto *value = sqlite3_column_text(statement, column);
  return value ? reinterpret_cast<const char *>(value) : std::string{};
}

std::filesystem::path absolutePath(std::filesystem::path path) {
  auto absolute =
      (path.is_absolute() ? std::move(path) : std::filesystem::absolute(path))
          .lexically_normal();
  std::error_code error;
  auto canonical = std::filesystem::weakly_canonical(absolute, error);
  return error ? absolute : canonical;
}

std::filesystem::path componentRoot(sqlite3_stmt *statement) {
  ProjectComponent component;
  component.name = columnText(statement, 1);
  component.path = columnText(statement, 2);
  if (const auto version = columnText(statement, 3); !version.empty()) {
    component.version = version;
  }
  if (sqlite3_column_type(statement, 4) != SQLITE_NULL) {
    component.repositoryId = sqlite3_column_int64(statement, 4);
  }
  const auto clonePath = columnText(statement, 5);
  const auto clone =
      clonePath.empty()
          ? std::optional<ProjectClone>{}
          : std::optional<ProjectClone>{ProjectClone{.path = clonePath}};
  return effectiveComponentRoot(component, clone);
}

std::expected<std::vector<Component>, std::string>
readComponents(sqlite3 *database) {
  constexpr auto sql =
      "SELECT component.id, component.name, component.path, "
      "component.version, component.repository_id, clone.path "
      "FROM component "
      "LEFT JOIN repository ON repository.id=component.repository_id "
      "LEFT JOIN clone ON clone.id=repository.active_clone_id "
      "ORDER BY component.id";
  return prepare(database, sql).and_then([&](Statement statement) {
    std::vector<Component> components;
    auto status = sqlite3_step(statement.get());
    while (status == SQLITE_ROW) {
      components.push_back({sqlite3_column_int64(statement.get(), 0),
                            columnText(statement.get(), 1),
                            componentRoot(statement.get())});
      status = sqlite3_step(statement.get());
    }
    if (status != SQLITE_DONE) {
      return std::expected<std::vector<Component>, std::string>{
          std::unexpected(sqliteError(database))};
    }
    return std::expected<std::vector<Component>, std::string>{
        std::move(components)};
  });
}

std::expected<std::vector<StoredCompileFile>, std::string>
readStoredFiles(sqlite3 *database) {
  constexpr auto sql =
      "SELECT component.id, component.name, component.path, "
      "component.version, component.repository_id, clone.path, "
      "directory.path, file.name, file.driver, file.working_directory, "
      "file.compile_options "
      "FROM file JOIN directory ON directory.id=file.directory_id "
      "JOIN component ON component.id=directory.component_id "
      "LEFT JOIN repository ON repository.id=component.repository_id "
      "LEFT JOIN clone ON clone.id=repository.active_clone_id "
      "WHERE file.compile_options IS NOT NULL ORDER BY file.id";
  return prepare(database, sql).and_then([&](Statement statement) {
    std::vector<StoredCompileFile> files;
    auto status = sqlite3_step(statement.get());
    while (status == SQLITE_ROW) {
      auto root = componentRoot(statement.get());
      files.push_back(
          {root,
           (root / columnText(statement.get(), 6) /
            columnText(statement.get(), 7))
               .lexically_normal(),
           columnText(statement.get(), 1), columnText(statement.get(), 8),
           columnText(statement.get(), 9), columnText(statement.get(), 10)});
      status = sqlite3_step(statement.get());
    }
    if (status != SQLITE_DONE) {
      return std::expected<std::vector<StoredCompileFile>, std::string>{
          std::unexpected(sqliteError(database))};
    }
    return std::expected<std::vector<StoredCompileFile>, std::string>{
        std::move(files)};
  });
}

bool hasTable(sqlite3 *database, std::string_view name) {
  constexpr auto sql =
      "SELECT EXISTS(SELECT 1 FROM sqlite_master WHERE type='table' "
      "AND name=?1)";
  auto statement = prepare(database, sql);
  if (!statement ||
      sqlite3_bind_text(statement->get(), 1, name.data(),
                        static_cast<int>(name.size()),
                        SQLITE_TRANSIENT) != SQLITE_OK ||
      sqlite3_step(statement->get()) != SQLITE_ROW) {
    return false;
  }
  return sqlite3_column_int(statement->get(), 0) != 0;
}

std::expected<Aliases, std::string> readLabels(sqlite3 *database) {
  if (!hasTable(database, "label")) {
    return Aliases{};
  }
  return prepare(database, "SELECT name,path FROM label ORDER BY name")
      .and_then([&](Statement statement) {
        Aliases labels;
        auto status = sqlite3_step(statement.get());
        while (status == SQLITE_ROW) {
          labels.insert_or_assign(columnText(statement.get(), 0),
                                  columnText(statement.get(), 1));
          status = sqlite3_step(statement.get());
        }
        if (status != SQLITE_DONE) {
          return std::expected<Aliases, std::string>{
              std::unexpected(sqliteError(database))};
        }
        return std::expected<Aliases, std::string>{std::move(labels)};
      });
}

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

Aliases componentAliases(const std::vector<Component> &components) {
  std::map<std::string, std::vector<std::filesystem::path>> grouped;
  for (const auto &component : components) {
    grouped[component.name].push_back(component.root);
  }
  Aliases aliases;
  for (const auto &[name, roots] : grouped) {
    if (const auto root = componentAliasRoot(roots)) {
      aliases.insert_or_assign(name, root->string());
    }
  }
  return aliases;
}

Aliases mergeAliases(Aliases explicitLabels, const Aliases &components) {
  for (const auto &[name, path] : components) {
    explicitLabels.try_emplace(name, path);
  }
  return explicitLabels;
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

std::size_t commandStart(const std::vector<std::string> &options) {
  auto found = std::ranges::find_if(options, [](const auto &option) {
    return !isEnvironmentAssignment(option) && !isLauncher(option);
  });
  return found == options.end()
             ? 0
             : static_cast<std::size_t>(std::distance(options.begin(), found));
}

std::vector<std::string> sanitize(std::vector<std::string> options) {
  std::vector<std::string> result;
  auto index = (!options.empty() && (isEnvironmentAssignment(options.front()) ||
                                     isLauncher(options.front())))
                   ? commandStart(options) + 1
                   : std::size_t{0};
  while (index < options.size()) {
    auto option = std::move(options[index++]);
    if (drop.contains(option) || hasDropPrefix(option)) {
      continue;
    }
    if (dropWithArgument.contains(option)) {
      index = std::min(index + 1, options.size());
      continue;
    }
    result.push_back(std::move(option));
  }
  return result;
}

std::string expandLabels(std::string value, const Aliases &aliases) {
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

std::string resolvePath(std::string value, const Aliases &aliases) {
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
  return absolutePath(value).string();
}

std::vector<std::string> resolveIncludePaths(std::vector<std::string> options,
                                             const Aliases &aliases) {
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

std::string defaultDriver(const std::filesystem::path &source) {
  return source.extension() == ".c" ? "clang" : "clang++";
}

std::expected<clang::tooling::CompileCommand, std::string>
toCompileCommand(const StoredCompileFile &file, const Aliases &aliases) {
  return decodeOptions(file.options).transform([&](auto options) {
    auto fileAliases = aliases;
    fileAliases.try_emplace(file.componentName, file.root.string());
    auto arguments =
        resolveIncludePaths(sanitize(std::move(options)), fileAliases);
    arguments.insert(arguments.begin(), file.driver.empty()
                                            ? defaultDriver(file.path)
                                            : file.driver);
    arguments.push_back(file.path.string());
    const auto directory = file.workingDirectory.empty()
                               ? file.root
                               : std::filesystem::path(resolvePath(
                                     file.workingDirectory, fileAliases));
    return clang::tooling::CompileCommand(
        directory.string(), file.path.string(), std::move(arguments), "");
  });
}

std::expected<Commands, std::string>
toCompileCommands(const std::vector<StoredCompileFile> &files,
                  const Aliases &aliases) {
  Commands commands;
  commands.reserve(files.size());
  for (const auto &file : files) {
    auto command = toCompileCommand(file, aliases);
    if (!command) {
      return std::unexpected(command.error());
    }
    commands.push_back(std::move(*command));
  }
  return commands;
}

class StoredCompilationDatabase final
    : public clang::tooling::CompilationDatabase {
public:
  explicit StoredCompilationDatabase(Commands commands)
      : commands_(std::move(commands)) {}

  std::vector<clang::tooling::CompileCommand>
  getCompileCommands(llvm::StringRef filePath) const override {
    const auto identity = absolutePath(filePath.str()).string();
    auto matching =
        commands_ | std::views::filter([&](const auto &command) {
          return absolutePath(command.Filename).string() == identity;
        });
    return matching | std::ranges::to<Commands>();
  }

  std::vector<std::string> getAllFiles() const override {
    auto files = commands_ | std::views::transform([](const auto &command) {
                   return command.Filename;
                 });
    return files | std::ranges::to<std::vector>();
  }

  Commands getAllCompileCommands() const override { return commands_; }

private:
  Commands commands_;
};

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

std::string encodeOptions(const std::vector<std::string> &options) {
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
  return absolutePath(path.is_absolute()
                          ? path
                          : std::filesystem::path(command.Directory) / path);
}

std::string normalizePathValue(const clang::tooling::CompileCommand &command,
                               std::string value,
                               const ProjectComponent &component,
                               const ProjectClone &clone) {
  const auto resolved = resolveCommandPath(command, value);
  return portablePath(resolved, component, clone);
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
  const auto compiler = compilerName && usablePath;
  return compiler ? candidate : defaultDriver(source);
}

std::vector<std::string>
normalizePathOptions(const clang::tooling::CompileCommand &command,
                     std::vector<std::string> options,
                     const ProjectComponent &component,
                     const ProjectClone &clone) {
  constexpr std::string_view pathFlags[] = {
      "-include-pch", "-iframework", "-idirafter", "-resource-dir",
      "--sysroot",    "-isystem",    "-iquote",    "-include",
      "-imacros",     "-isysroot",   "-F",         "-I"};
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
            command, std::move(options[index++]), component, clone));
      }
      continue;
    }
    auto value = option.substr(flag.size());
    if (value.starts_with('=')) {
      value.erase(0, 1);
    }
    result.push_back(
        std::string(flag) + (flag.starts_with("--") ? "=" : "") +
        normalizePathValue(command, std::move(value), component, clone));
  }
  return result;
}

std::filesystem::path
commonRoot(const std::vector<clang::tooling::CompileCommand> &commands) {
  auto root = absolutePath(commands.front().Directory);
  for (const auto &command : commands) {
    auto directory = absolutePath(command.Directory);
    while (!ownsPath(root, directory)) {
      const auto parent = root.parent_path();
      if (parent == root) {
        return root;
      }
      root = parent;
    }
    auto source = resolveCommandPath(command, command.Filename);
    while (!ownsPath(root, source)) {
      const auto parent = root.parent_path();
      if (parent == root) {
        return root;
      }
      root = parent;
    }
  }
  return root;
}

std::optional<std::filesystem::path> gitRoot(std::filesystem::path directory) {
  auto current = absolutePath(std::move(directory));
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

std::string repositoryName(const std::filesystem::path &cloneRoot) {
  auto name = cloneRoot.filename().string();
  if (name.empty()) {
    name = cloneRoot.parent_path().filename().string();
  }
  return name;
}

ProjectComponent normalizeComponent(ProjectComponent component,
                                    const ProjectClone &clone) {
  const auto path = std::filesystem::path(component.path);
  const auto resolved = absolutePath(
      path.is_absolute() ? path : std::filesystem::path(clone.path) / path);
  const auto cloneRoot = absolutePath(clone.path);
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

std::expected<PreparedCommand, std::string>
prepareCommand(const clang::tooling::CompileCommand &command,
               std::span<const ProjectComponent> components,
               const ProjectClone &clone) {
  if (command.CommandLine.empty()) {
    return std::unexpected("compile command has no arguments");
  }
  const auto source = resolveCommandPath(command, command.Filename);
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
  std::erase_if(options, [&](const std::string &option) {
    return option == command.Filename || option == source.string() ||
           (!option.starts_with('-') &&
            resolveCommandPath(command, option) == source);
  });
  auto sanitized = sanitize(std::move(options));
  sanitized = normalizePathOptions(command, std::move(sanitized),
                                   components[*component], clone);
  const auto root = effectiveComponentRoot(components[*component], clone);
  const auto directory = source.lexically_relative(root).parent_path();
  const auto workingDirectory =
      portablePath(resolveCommandPath(command, command.Directory),
                   components[*component], clone);
  std::ostringstream key;
  key << source.string() << '\x1f' << command.Directory << '\x1f'
      << command.CommandLine[start];
  for (const auto &option : sanitized) {
    key << '\x1f' << option;
  }
  return PreparedCommand{
      source,           *component,
      key.str(),        compilerDriver(command, start, source),
      workingDirectory, std::move(sanitized)};
}

} // namespace

std::expected<std::unique_ptr<clang::tooling::CompilationDatabase>, std::string>
loadStoredCompilationDatabase(std::string databasePath) {
  return openReadOnly(absolutePath(std::move(databasePath)))
      .and_then([](Connection connection) {
        return readComponents(connection.handle)
            .and_then([&](const std::vector<Component> &components) {
              return readLabels(connection.handle)
                  .transform([&](Aliases labels) {
                    return mergeAliases(std::move(labels),
                                        componentAliases(components));
                  });
            })
            .and_then([&](const Aliases &aliases) {
              return readStoredFiles(connection.handle)
                  .and_then([&](const std::vector<StoredCompileFile> &files) {
                    return toCompileCommands(files, aliases);
                  });
            })
            .transform([](Commands commands) {
              return std::unique_ptr<clang::tooling::CompilationDatabase>(
                  std::make_unique<StoredCompilationDatabase>(
                      std::move(commands)));
            });
      });
}

namespace {

Commands selectCommands(const clang::tooling::CompilationDatabase &database,
                        std::span<const std::string> requestedSources) {
  if (requestedSources.empty()) {
    return database.getAllCompileCommands();
  }
  Commands commands;
  for (const auto &source : requestedSources) {
    auto selected = database.getCompileCommands(source);
    commands.insert(commands.end(), selected.begin(), selected.end());
  }
  return commands;
}

} // namespace

std::expected<ProjectImportResult, std::string>
importProjectConfiguration(FileManager &files,
                           const clang::tooling::CompilationDatabase &database,
                           std::span<const std::string> requestedSources,
                           const ProjectImportOptions &options) {
  auto commands = selectCommands(database, requestedSources);
  if (commands.empty()) {
    return std::unexpected("compilation database contains no commands");
  }

  ProjectConfiguration configuration;
  const auto commandRoot = commonRoot(commands);
  configuration.activeClone.path =
      gitRoot(commandRoot).value_or(commandRoot).string();
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
    auto prepared = prepareCommand(command, configuration.components,
                                   configuration.activeClone);
    if (!prepared) {
      return std::unexpected(prepared.error());
    }
    auto candidate = std::move(*prepared);
    const auto source = candidate.source.string();
    const auto entry = selected.find(source);
    if (entry == selected.end()) {
      selected.emplace(source, std::move(candidate));
    } else {
      ++result.duplicateCommands;
      result.diagnostics.push_back("duplicate compile command for " + source +
                                   "; selected the deterministic winner");
      if (entry->second.key > candidate.key) {
        entry->second = std::move(candidate);
      }
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
                    .compileOptions = encodeOptions(command.options)});
  }
  result.importedFiles = configuration.files.size();
  if (auto imported = files.replaceProjectConfiguration(configuration);
      !imported) {
    return std::unexpected("cannot store project configuration: " +
                           imported.error().message());
  }
  return result;
}

} // namespace facts
