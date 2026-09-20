#include "apis/v2/catalog/Internal.h"
#include "tooling/CompilationCommandCodec.h"

namespace facts::apis::v2::catalog {
Result<StoredCompilationSnapshot> compilationContext(Database &database) {
  return lift(native::components(database)).and_then([&](const auto &components) -> Result<StoredCompilationSnapshot> {
    StoredCompilationSnapshot context;
    for (const auto &component : components) {
      auto root = lift(native::componentRoot(component));
      if (!root) return std::unexpected(root.error());
      context.components.push_back({component.value.id, component.value.name, *root});
    }
    auto exists = lift(native::query(database,
        "SELECT EXISTS(SELECT 1 FROM sqlite_master WHERE type='table' AND name='label')",
        [](const storage::Row &row) { return row.integer(0) != 0; }));
    if (!exists) return std::unexpected(exists.error());
    if (exists->front()) {
      auto labels = lift(native::query(database, "SELECT name,path FROM label ORDER BY name",
          [](const storage::Row &row) { return std::pair{row.string(0), row.string(1)}; }));
      if (!labels) return std::unexpected(labels.error());
      for (const auto &[name, path] : *labels) context.labels[name] = path;
    }
    return context;
  });
}
Result<Json> compilationCommand(const native::File &file, const StoredCompilationSnapshot &context) {
  if (file.driver.empty()) return Json(nullptr);
  return lift(native::filePath(file)).and_then([&](const auto &path) -> Result<Json> {
    auto snapshot = context;
    snapshot.files.push_back({effectiveComponentRoot(file.component, file.clone), path,
        file.componentName, file.driver, file.workingDirectory, file.compileOptions,
        static_cast<FileId>(file.id), {}});
    return lift(expandCompileCommands(snapshot)).transform([&](auto commands) {
      auto &command = commands.front();
      auto arguments = std::move(command.CommandLine);
      arguments.erase(arguments.begin()); // Driver has its own typed field.
      std::erase(arguments, path.string()); // Source is the file resource itself.
      return Json{{"driver", file.driver}, {"working_directory", command.Directory},
                  {"arguments", std::move(arguments)}};
    });
  });
}
}
