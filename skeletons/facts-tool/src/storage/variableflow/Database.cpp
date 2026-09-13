#include "storage/variableflow/Database.h"

#include "storage/Sqlite.h"
#include "storage/catalog/Database.h"
#include "storage/variableflow/Schema.h"

#include <chrono>
#include <format>
#include <numeric>
#include <sqlite3.h>
#include <utility>

namespace facts::variableflow {
namespace {
using facts::storage::Database;

std::string now() {
  return std::format("{:%FT%TZ}", std::chrono::floor<std::chrono::seconds>(
                                      std::chrono::system_clock::now()));
}

std::string join(const std::vector<std::string> &values) {
  return std::accumulate(values.begin(), values.end(), std::string{},
                         [](std::string out, const std::string &value) {
                           return out.empty() ? value : out + "\n" + value;
                         });
}

template <typename Value>
std::optional<std::int64_t> integer(const std::optional<Value> &value) {
  return value ? std::optional<std::int64_t>{static_cast<std::int64_t>(*value)}
               : std::nullopt;
}

std::expected<std::int64_t, std::string>
insertRun(Database &database, const RunMetadata &metadata, const Graph &graph) {
  return catalog::execute(
             database,
             "INSERT INTO variable_flow_run(created_at,project_path,facts_path,"
             "function_selector,variable_selector,sources,declaration_line,max_"
             "depth,"
             "engine,assumptions,root_function,root_variable,status)"
             " VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?)",
             now(), metadata.projectPath, metadata.factsPath,
             metadata.functionSelector, metadata.variableSelector,
             join(metadata.sourceSelectors), integer(metadata.declarationLine),
             integer(metadata.maxDepth), metadata.engine, metadata.assumptions,
             graph.rootFunction, graph.rootVariable, graph.status)
      .transform([&] {
        return static_cast<std::int64_t>(
            sqlite3_last_insert_rowid(database.nativeHandle()));
      });
}

std::expected<void, std::string>
insertNodes(Database &database, std::int64_t runId, const Graph &graph) {
  for (const auto &node : graph.nodes) {
    auto result = catalog::execute(
        database,
        "INSERT INTO variable_flow_node VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?)",
        runId, node.id, node.kind, node.functionUsr, node.variableUsr,
        node.name, node.type, node.location.file,
        static_cast<std::int64_t>(node.location.line),
        static_cast<std::int64_t>(node.location.column),
        static_cast<std::int64_t>(node.location.offset), node.block,
        static_cast<std::int64_t>(node.depth));
    if (!result)
      return std::unexpected(result.error());
  }
  return {};
}
} // namespace

std::expected<std::int64_t, std::string>
persist(const std::filesystem::path &path, const RunMetadata &metadata,
        const Graph &graph) {
  std::error_code error;
  if (!path.parent_path().empty())
    std::filesystem::create_directories(path.parent_path(), error);
  if (error)
    return std::unexpected("cannot create variable-flow output directory: " +
                           error.message());
  return Database::open(path.string(), Database::readWrite)
      .transform_error([](auto value) { return value.message(); })
      .and_then([](Database database) -> std::expected<Database, std::string> {
        return storage::prepare(database).transform(
            [&] { return std::move(database); });
      })
      .and_then([&](Database database) {
        return database.write()
            .transform_error([](auto value) { return value.message(); })
            .and_then([&](auto transaction)
                          -> std::expected<std::int64_t, std::string> {
              return storage::initialize(database)
                  .and_then(
                      [&] { return insertRun(database, metadata, graph); })
                  .and_then([&](auto id) {
                    return insertNodes(database, id, graph).transform([=] {
                      return id;
                    });
                  })
                  .and_then([&](auto id) {
                    for (const auto &edge : graph.edges) {
                      auto result = catalog::execute(
                          database,
                          "INSERT INTO variable_flow_edge VALUES(?,?,?,?,?)",
                          id, edge.source, edge.target, edge.kind,
                          edge.callsite);
                      if (!result)
                        return std::expected<std::int64_t, std::string>{
                            std::unexpected(result.error())};
                    }
                    for (const auto &boundary : graph.boundaries) {
                      auto result = catalog::execute(
                          database,
                          "INSERT INTO variable_flow_boundary "
                          "VALUES(?,?,?,?,?)",
                          id, boundary.node, boundary.reason, boundary.detail,
                          static_cast<std::int64_t>(boundary.depth));
                      if (!result)
                        return std::expected<std::int64_t, std::string>{
                            std::unexpected(result.error())};
                    }
                    return transaction.commit()
                        .transform_error(
                            [](auto value) { return value.message(); })
                        .transform([=] { return id; });
                  });
            });
      });
}
} // namespace facts::variableflow
