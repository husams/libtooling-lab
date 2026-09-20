#include "apis/v2/JobServices.h"
#include "apis/operations/Diagnostics.h"
#include "apis/watch/catalog/Ownership.h"
#include "apis/watch/CompilationValidation.h"
#include "commands/Import.h"
#include "tooling/import/Identity.h"
#include "model/AnalysisDiagnostic.h"
#include "storage/catalog/Database.h"
#include <atomic>
#include <clang/Tooling/CompilationDatabase.h>

namespace facts::apis::v2::jobs {
namespace {
Result<watch::Scan> discover(const domain::Context &context, const runtime::Request &request) {
  return selectClones(context, request).and_then([&](watch::Catalog catalog) -> Result<watch::Scan> {
    auto allowed = checkpoint(request);
    if (!allowed) return std::unexpected(allowed.error());
    const std::atomic_bool cancelled{false};
    auto settings = request.settings;
    settings.defaults.insert(settings.defaults.end(), {"--conf", context.configuration.database.string()});
    return watch::discover(settings, std::move(catalog), cancelled).transform_error(failed)
        .and_then([&](watch::Scan scan) -> Result<watch::Scan> {
          auto allowed = checkpoint(request);
          if (!allowed) return std::unexpected(allowed.error());
          return scan;
        });
  });
}
Result<std::size_t> fileCount(const domain::Context &context) {
  return catalog::open(context.configuration.database.string(), false)
      .and_then([](catalog::Database database) {
        return catalog::query(database, "SELECT COUNT(*) FROM file",
            [](const storage::Row &row) { return static_cast<std::size_t>(row.integer(0)); });
      }).transform([](const auto &counts) { return counts.front(); }).transform_error(failed);
}
Result<std::size_t> importOne(const domain::Context &context, const std::filesystem::path &path,
                               const watch::Clone &clone, const runtime::Request &request) {
  cli::ImportOptions options;
  options.configuration = context.configuration.database.string();
  for (std::size_t i = 0; i + 1 < request.settings.defaults.size(); i += 2)
    if (request.settings.defaults[i] == "--config") options.configurationFile = request.settings.defaults[i + 1];
  options.existingClone = clone.cloneId;
  options.compilationDatabase = path.string();
  options.defaultExtraArguments = context.configuration.extraArguments;
  options.astCache = context.configuration.astCache;
  std::string error;
  auto compilation = clang::tooling::CompilationDatabase::loadFromDirectory(path.string(), error);
  if (!compilation) return std::unexpected(failed(error));
  options.sources = compilation->getAllFiles();
  auto configuration = context.configuration;
  auto identity = readImportIdentity(configuration.database, clone.cloneId);
  if (!identity) return std::unexpected(failed(identity.error()));
  configuration.projectRoot = identity->activeClone.path;
  return fileCount(context).and_then([&](std::size_t before) {
    return commands::runImportResolved(options, configuration).transform_error(failed)
        .and_then([&](int status) -> Result<std::size_t> {
          if (status != 0) return std::unexpected(failed("Compilation import failed"));
          return fileCount(context).transform([&](std::size_t after) { return after - before; });
        });
  });
}
}
Result<Json> scan(const domain::Context &context, const runtime::Request &request) {
  return discover(context, request).transform([](const watch::Scan &scan) {
    Json warnings = Json::array(), databases = Json::array();
    bool incomplete = !scan.notices.empty();
    for (const auto &warning : scan.warnings) {
      warnings.push_back({{"code", warning.code}, {"severity", "warning"},
          {"path", warning.path.string()},
          {"target", warning.target.empty() ? Json(nullptr) : Json(warning.target.string())},
          {"message", warning.message}, {"action", "skipped"}});
      incomplete = incomplete || warning.code == "unreadable_directory" ||
                   warning.code == "unavailable_entry";
    }
    for (const auto &notice : scan.notices)
      warnings.push_back({{"code", "scan_notice"}, {"severity", "warning"},
          {"path", ""}, {"target", nullptr}, {"message", notice}, {"action", "skipped"}});
    for (const auto &path : scan.databases) databases.push_back({{"path", (path / "compile_commands.json").string()}});
    return Json{{"directories_scanned", scan.directories.size()},
        {"compilation_databases", scan.databases.size()}, {"warning_count", warnings.size()},
        {"coverage", incomplete ? "partial" : "complete"}, {"databases", std::move(databases)},
        {"warnings", std::move(warnings)}, {"diagnostics", Json::array()}};
  });
}
Result<Json> importCompilation(const domain::Context &context, const runtime::Request &request) {
  return operations::withDiagnostics([&]() -> Result<Json> {
    return discover(context, request).and_then([&](watch::Scan scan) -> Result<Json> {
      for (const auto &warning : scan.warnings)
        collectDiagnostic({"warning", "[" + warning.code + "] " + warning.message,
                           warning.path.string(), 0, 0});
      for (const auto &notice : scan.notices)
        collectDiagnostic({"warning", notice, "", 0, 0});
      if (request.options.contains("compilation_database")) {
        auto path = std::filesystem::path(request.options["compilation_database"].get<std::string>());
        if (path.is_relative()) {
          if (scan.catalog.clones.size() != 1)
            return std::unexpected(invalid("relative compilation_database requires one repository"));
          path = scan.catalog.clones.front().path / path;
        }
        if (path.filename() == "compile_commands.json") path = path.parent_path();
        scan.databases = {path.lexically_normal()};
      }
      if (scan.databases.empty()) return std::unexpected(domain::Error{422,
          "needs_configuration", "No compilation database was found in the selected repositories"});
      auto compatible = watch::validateCompilationDatabases(scan.databases);
      if (!compatible) return std::unexpected(domain::Error{409,
          "ambiguous_compilation_database", compatible.error()});
      Json databases = Json::array();
      std::size_t registered = 0;
      for (const auto &path : scan.databases) {
        auto allowed = checkpoint(request);
        if (!allowed) return std::unexpected(allowed.error());
        const auto *clone = watch::owner(path, scan.catalog);
        if (!clone) return std::unexpected(invalid("Compilation database is outside the selected registered clones"));
        auto imported = importOne(context, path, *clone, request);
        if (!imported) return std::unexpected(imported.error());
        registered += *imported;
        databases.push_back({{"path", (path / "compile_commands.json").string()}, {"files_registered", *imported}});
      }
      return Json{{"compilation_databases", databases.size()}, {"files_registered", registered},
                  {"databases", std::move(databases)}};
    });
  });
}
}
