#include "tooling/StoredCompilationDatabase.h"

#include <clang/Tooling/CompilationDatabasePluginRegistry.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/JSON.h>

#include <sqlite3.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <map>
#include <optional>
#include <ranges>
#include <regex>
#include <set>
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

struct StoredFile {
  std::filesystem::path root;
  std::filesystem::path path;
  std::string driver;
  std::string options;
};

std::string configuredDatabasePath;
std::optional<std::string> configuredDatabaseError;

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
  const auto componentPath = std::filesystem::path(columnText(statement, 2));
  const auto version = columnText(statement, 3);
  const auto repositoryId = sqlite3_column_type(statement, 4) != SQLITE_NULL;
  const auto clonePath = columnText(statement, 5);
  const auto effective =
      version.empty() ? componentPath : componentPath / version;
  const auto cloneAnchored = repositoryId && componentPath.is_relative() &&
                             !clonePath.empty() &&
                             !componentPath.string().contains('<') &&
                             !componentPath.string().contains('$');
  return absolutePath(
      cloneAnchored ? std::filesystem::path(clonePath) / effective : effective);
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

std::expected<std::vector<StoredFile>, std::string>
readStoredFiles(sqlite3 *database) {
  constexpr auto sql =
      "SELECT component.id, component.name, component.path, "
      "component.version, component.repository_id, clone.path, "
      "directory.path, file.name, file.driver, file.compile_options "
      "FROM file JOIN directory ON directory.id=file.directory_id "
      "JOIN component ON component.id=directory.component_id "
      "LEFT JOIN repository ON repository.id=component.repository_id "
      "LEFT JOIN clone ON clone.id=repository.active_clone_id "
      "WHERE file.compile_options IS NOT NULL ORDER BY file.id";
  return prepare(database, sql).and_then([&](Statement statement) {
    std::vector<StoredFile> files;
    auto status = sqlite3_step(statement.get());
    while (status == SQLITE_ROW) {
      auto root = componentRoot(statement.get());
      files.push_back({root,
                       (root / columnText(statement.get(), 6) /
                        columnText(statement.get(), 7))
                           .lexically_normal(),
                       columnText(statement.get(), 8),
                       columnText(statement.get(), 9)});
      status = sqlite3_step(statement.get());
    }
    if (status != SQLITE_DONE) {
      return std::expected<std::vector<StoredFile>, std::string>{
          std::unexpected(sqliteError(database))};
    }
    return std::expected<std::vector<StoredFile>, std::string>{
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
  constexpr std::string_view includeFlags[] = {"-I", "-isystem", "-iquote"};
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
toCompileCommand(const StoredFile &file, const Aliases &aliases) {
  return decodeOptions(file.options).transform([&](auto options) {
    auto arguments = resolveIncludePaths(sanitize(std::move(options)), aliases);
    arguments.insert(arguments.begin(), file.driver.empty()
                                            ? defaultDriver(file.path)
                                            : file.driver);
    arguments.push_back(file.path.string());
    return clang::tooling::CompileCommand(
        file.root.string(), file.path.string(), std::move(arguments), "");
  });
}

std::expected<Commands, std::string>
toCompileCommands(const std::vector<StoredFile> &files,
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

bool hasJsonCompilationDatabase(std::filesystem::path directory) {
  std::error_code error;
  directory = absolutePath(std::move(directory));
  return std::filesystem::is_regular_file(directory / "compile_commands.json",
                                          error);
}

class StoredCompilationDatabasePlugin final
    : public clang::tooling::CompilationDatabasePlugin {
public:
  std::unique_ptr<clang::tooling::CompilationDatabase>
  loadFromDirectory(llvm::StringRef directory,
                    std::string &errorMessage) override {
    if (configuredDatabasePath.empty() ||
        hasJsonCompilationDatabase(directory.str())) {
      return nullptr;
    }
    std::error_code pathError;
    if (!std::filesystem::is_regular_file(configuredDatabasePath, pathError)) {
      return nullptr;
    }
    auto database = loadStoredCompilationDatabase(configuredDatabasePath);
    if (!database) {
      errorMessage = database.error();
      configuredDatabaseError = database.error();
      return nullptr;
    }
    if ((*database)->getAllFiles().empty()) {
      return nullptr;
    }
    return std::move(*database);
  }
};

static clang::tooling::CompilationDatabasePluginRegistry::Add<
    StoredCompilationDatabasePlugin>
    storedDatabasePlugin("facts-stored-compile-options",
                         "Reads compile options from a cpp-indexer database");

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
                  .and_then([&](const std::vector<StoredFile> &files) {
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

void configureStoredCompilationDatabase(std::string databasePath) {
  configuredDatabasePath = absolutePath(std::move(databasePath)).string();
  configuredDatabaseError.reset();
}

std::optional<std::string> storedCompilationDatabaseError() {
  return configuredDatabaseError;
}

} // namespace facts
