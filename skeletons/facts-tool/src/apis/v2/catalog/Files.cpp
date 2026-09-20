#include "apis/v2/catalog/Internal.h"
#include <cstdlib>

namespace facts::apis::v2::catalog {
namespace {
struct CompilationCommand {
  std::string driver;
  std::string workingDirectory;
  std::string arguments;
};
Result<std::string> driverPath(const std::string &driver) {
  std::error_code error;
  const std::filesystem::path requested(driver);
  if (requested.has_parent_path()) {
    auto path = std::filesystem::canonical(requested, error);
    if (!error && std::filesystem::is_regular_file(path, error)) return path.string();
    return std::unexpected(invalid("Compiler driver is not a regular file"));
  }
  const auto *environment = std::getenv("PATH");
  std::string_view remaining = environment ? environment : "";
  while (!remaining.empty()) {
    const auto colon = remaining.find(':');
    const auto directory = remaining.substr(0, colon);
    auto candidate = std::filesystem::path(directory) / driver;
    if (!directory.empty() && std::filesystem::is_regular_file(candidate, error)) {
      auto normalized = std::filesystem::canonical(candidate, error);
      if (!error) return normalized.string();
    }
    remaining = colon == std::string_view::npos ? std::string_view{} : remaining.substr(colon + 1);
  }
  return std::unexpected(invalid("Compiler driver was not found on the server PATH"));
}
Result<CompilationCommand> command(const Json &body) {
  return fields(body, {"driver", "working_directory", "arguments"}).and_then([&]() -> Result<CompilationCommand> {
    auto driver = text(body, "driver", true);
    auto working = text(body, "working_directory", true);
    if (!driver) return std::unexpected(driver.error());
    if (!working) return std::unexpected(working.error());
    if (!body.contains("arguments") || !body.at("arguments").is_array() ||
        body.at("arguments").size() > 4096 ||
        !std::ranges::all_of(body.at("arguments"), [](const Json &argument) {
          return argument.is_string() && argument.get_ref<const std::string &>().size() <= 65536 &&
                 argument.get_ref<const std::string &>().find('\0') == std::string::npos;
        })) return std::unexpected(invalid("arguments must contain at most 4096 strings, each at most 65536 bytes and without NUL characters"));
    return driverPath(*driver).and_then([&](const auto &resolvedDriver) {
      return lift(native::existingDirectory(*working)).transform([&](const auto &resolvedWorking) {
        return CompilationCommand{resolvedDriver, resolvedWorking.string(), body.at("arguments").dump()};
      });
    });
  });
}
Result<Response> create(Database &database, const Json &body) {
  return fields(body, {"path", "compilation_command"}).and_then([&]() -> Result<Response> {
    auto path = text(body, "path", true);
    if (!path) return std::unexpected(path.error());
    if (!body.contains("compilation_command")) return std::unexpected(invalid("Missing field: compilation_command"));
    return command(body.at("compilation_command")).and_then([&](const auto &value) {
      return lift(native::addFile(database, *path, value.driver, value.workingDirectory, value.arguments));
    }).and_then([&] { return lift(native::file(database, *path)); })
      .and_then([&](const auto &value) { return encode(database, value); })
      .transform([](Json value) { return modified(std::move(value), "files", true); });
  });
}
Result<Response> update(Database &database, const native::File &value, const Json &body) {
  return fields(body, {"compilation_command"}).and_then([&]() -> Result<Response> {
    if (!body.contains("compilation_command")) return encode(database, value).transform([](Json item) { return modified(std::move(item), "files", false); });
    return command(body.at("compilation_command")).and_then([&](const auto &command) {
      if (value.driver == command.driver && value.workingDirectory == command.workingDirectory && value.compileOptions == command.arguments)
        return lift(native::execute(database, "UPDATE file SET args_overridden=1 WHERE id=?", value.id));
      return lift(native::execute(database,
          "INSERT OR IGNORE INTO api_index_invalidated_file(file_id) VALUES(?)", value.id))
          .and_then([&] { return lift(native::execute(database, "UPDATE file SET driver=?,working_directory=?,compile_options=?,"
          "args_overridden=1,indexed=0,indexed_at=NULL WHERE id=?",
          command.driver, command.workingDirectory, command.arguments, value.id)); });
    }).and_then([&] { return file(database, value.id); })
      .and_then([&](const auto &value) { return encode(database, value); })
      .transform([](Json value) { return modified(std::move(value), "files", false); });
  });
}
}
Result<Response> files(Database &database, std::string_view method, std::string_view id,
                       const Json &body, const Json &query) {
  if (id.empty() && method == "POST") return create(database, body);
  if (id.empty() && method == "GET")
    return lift(native::files(database)).and_then([&](const auto &values) -> Result<Json> {
      auto owners = lift(native::repositories(database));
      if (!owners) return std::unexpected(owners.error());
      auto context = compilationContext(database);
      if (!context) return std::unexpected(context.error());
      Json items = Json::array();
      for (const auto &value : values) {
        if (query.contains("component") && value.componentName != query.at("component").get<std::string>()) continue;
        if (query.contains("repository") && !std::ranges::any_of(*owners, [&](const auto &owner) {
              return value.component.repositoryId == owner.id && owner.name == query.at("repository").get<std::string>();
            })) continue;
        auto item = encode(value, *context);
        if (!item) return std::unexpected(item.error());
        items.push_back(std::move(*item));
      }
      return page(std::move(items), query);
    }).transform([](Json value) { return Response{200, std::move(value)}; });
  if (!id.empty() && (method == "GET" || method == "PATCH" || method == "DELETE"))
    return identifier(id).and_then([&](auto value) { return file(database, value); })
        .and_then([&](const auto &value) -> Result<Response> {
          if (method == "GET") return encode(database, value).transform([](Json item) { return Response{200, std::move(item)}; });
          if (method == "PATCH") return update(database, value, body);
          return lift(native::removeFile(database, value.id)).transform([] { return Response{204, nullptr, {}, true}; });
        });
  return std::unexpected(domain::Error{405, "method_not_allowed", "Unsupported file operation"});
}
}
