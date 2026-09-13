#include "storage/variableflow/Schema.h"

#include "storage/catalog/Database.h"

#include <algorithm>
#include <map>
#include <ranges>
#include <set>
#include <string_view>

namespace facts::variableflow::storage {
namespace {
constexpr std::string_view schema = R"sql(
CREATE TABLE variable_flow_run(
 run_id INTEGER PRIMARY KEY, created_at TEXT NOT NULL,
 project_path TEXT NOT NULL, facts_path TEXT NOT NULL,
 function_selector TEXT NOT NULL, variable_selector TEXT NOT NULL,
 sources TEXT NOT NULL,
 declaration_line INTEGER, max_depth INTEGER, engine TEXT NOT NULL,
 assumptions TEXT NOT NULL, root_function TEXT NOT NULL,
 root_variable TEXT NOT NULL, status TEXT NOT NULL CHECK(status <> '')
);
CREATE TABLE variable_flow_node(
 run_id INTEGER NOT NULL REFERENCES variable_flow_run(run_id) ON DELETE CASCADE,
 node_id INTEGER NOT NULL, kind TEXT NOT NULL, function_usr TEXT NOT NULL,
 variable_usr TEXT NOT NULL, name TEXT NOT NULL, type TEXT NOT NULL,
 file TEXT NOT NULL, line INTEGER NOT NULL, column_no INTEGER NOT NULL,
 offset INTEGER NOT NULL, block INTEGER NOT NULL, depth INTEGER NOT NULL,
 PRIMARY KEY(run_id,node_id)
);
CREATE TABLE variable_flow_edge(
 run_id INTEGER NOT NULL REFERENCES variable_flow_run(run_id) ON DELETE CASCADE,
 source INTEGER NOT NULL, target INTEGER NOT NULL, kind TEXT NOT NULL,
 callsite INTEGER NOT NULL,
 FOREIGN KEY(run_id,source) REFERENCES variable_flow_node(run_id,node_id),
 FOREIGN KEY(run_id,target) REFERENCES variable_flow_node(run_id,node_id)
);
CREATE TABLE variable_flow_boundary(
 run_id INTEGER NOT NULL REFERENCES variable_flow_run(run_id) ON DELETE CASCADE,
 node INTEGER NOT NULL, reason TEXT NOT NULL, detail TEXT NOT NULL,
 depth INTEGER NOT NULL,
 FOREIGN KEY(run_id,node) REFERENCES variable_flow_node(run_id,node_id)
);
PRAGMA user_version=1;
)sql";

const std::set<std::string> tables{"variable_flow_boundary",
                                   "variable_flow_edge", "variable_flow_node",
                                   "variable_flow_run"};
const std::map<std::string, std::set<std::string>> columns{
    {"variable_flow_run",
     {"run_id", "created_at", "project_path", "facts_path", "function_selector",
      "variable_selector", "sources", "declaration_line", "max_depth", "engine",
      "assumptions", "root_function", "root_variable", "status"}},
    {"variable_flow_node",
     {"run_id", "node_id", "kind", "function_usr", "variable_usr", "name",
      "type", "file", "line", "column_no", "offset", "block", "depth"}},
    {"variable_flow_edge", {"run_id", "source", "target", "kind", "callsite"}},
    {"variable_flow_boundary",
     {"run_id", "node", "reason", "detail", "depth"}}};

std::expected<int, std::string> version(Database &database) {
  return catalog::query(
             database, "PRAGMA user_version",
             [](const Row &row) { return static_cast<int>(row.integer(0)); })
      .and_then([](const auto &rows) -> std::expected<int, std::string> {
        return rows.empty()
                   ? std::unexpected("cannot read variable-flow schema")
                   : std::expected<int, std::string>{rows.front()};
      });
}

std::expected<std::set<std::string>, std::string>
tableNames(Database &database) {
  return catalog::query(database,
                        "SELECT name FROM sqlite_master WHERE type='table'",
                        [](const Row &row) { return row.string(0); })
      .transform(
          [](auto names) { return std::set(names.begin(), names.end()); });
}

std::expected<bool, std::string> validColumns(Database &database,
                                              std::string_view table) {
  return catalog::query(
             database, "SELECT name FROM pragma_table_info(?)",
             [](const Row &row) { return row.string(0); }, table)
      .transform([&](const auto &actual) {
        const auto &required = columns.at(std::string(table));
        return std::ranges::all_of(required, [&](const auto &name) {
          return std::ranges::find(actual, name) != actual.end();
        });
      });
}
} // namespace

std::expected<void, std::string> requireCurrent(Database &database) {
  return version(database).and_then([&](int current) {
    if (current != schemaVersion)
      return std::expected<void, std::string>{
          std::unexpected("unsupported variable-flow database schema version " +
                          std::to_string(current))};
    return tableNames(database).and_then([&](const auto &actual) {
      if (actual != tables)
        return std::expected<void, std::string>{
            std::unexpected("variable-flow database has an alien schema")};
      for (const auto &table : tables) {
        auto shape = validColumns(database, table);
        if (!shape)
          return std::expected<void, std::string>{
              std::unexpected(shape.error())};
        if (!*shape)
          return std::expected<void, std::string>{
              std::unexpected("variable-flow database has an alien schema")};
      }
      return std::expected<void, std::string>{};
    });
  });
}

std::expected<void, std::string> prepare(Database &database) {
  return database.executeScript("PRAGMA foreign_keys=ON;")
      .transform_error([](auto error) { return error.message(); })
      .and_then([&] { return version(database); })
      .and_then([&](int current) -> std::expected<void, std::string> {
        return tableNames(database).and_then([&](const auto &actual) {
          if (current == 0 && actual.empty())
            return std::expected<void, std::string>{};
          return requireCurrent(database);
        });
      });
}

std::expected<void, std::string> initialize(Database &database) {
  return version(database).and_then([&](int current) {
    return tableNames(database).and_then([&](const auto &actual) {
      if (current == 0 && actual.empty())
        return database.executeScript(schema).transform_error(
            [](auto error) { return error.message(); });
      return requireCurrent(database);
    });
  });
}
} // namespace facts::variableflow::storage
