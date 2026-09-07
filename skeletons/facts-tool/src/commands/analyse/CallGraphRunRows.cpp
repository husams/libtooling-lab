#include "commands/analyse/CallGraphRunRows.h"

#include "storage/catalog/Database.h"

#include <chrono>
#include <format>
#include <numeric>

namespace facts::commands {
namespace {
std::string joinComponents(const std::vector<std::string> &components) {
  return std::accumulate(components.begin(), components.end(), std::string{},
                         [](std::string joined, const std::string &name) {
                           return joined.empty() ? name : joined + "," + name;
                         });
}

std::string utcNow() {
  return std::format("{:%FT%TZ}", std::chrono::floor<std::chrono::seconds>(
                                      std::chrono::system_clock::now()));
}

template <typename Value>
std::optional<std::int64_t> integer(const std::optional<Value> &value) {
  if (!value)
    return std::nullopt;
  return static_cast<std::int64_t>(*value);
}

std::optional<std::string> text(const std::string &value) {
  return value.empty() ? std::nullopt : std::optional{value};
}
} // namespace

std::expected<std::int64_t, std::string>
insertCallGraphRun(storage::Database &database,
                   const CallGraphRunRecord &record) {
  const auto pathMode = record.pathMode.transform(
      [](auto mode) { return std::string{callgraph::pathModeName(mode)}; });
  const auto time = record.limits.time.transform(
      [](auto value) { return static_cast<std::int64_t>(value.count()); });
  return catalog::execute(
             database,
             "INSERT INTO callgraph_run(created_at,project_path,facts_path,"
             "mode,path_mode,calls_scope,components,max_depth,max_nodes,"
             "max_edges,time_limit_ms,recover_missing,status,"
             "truncation_reason,error) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)",
             utcNow(), record.projectPath, record.factsPath,
             callgraph::queryModeName(record.mode), pathMode,
             callgraph::scopeName(record.callsScope),
             joinComponents(record.components), integer(record.limits.depth),
             integer(record.limits.nodes), integer(record.limits.edges), time,
             record.recoverMissing ? 1 : 0, runStatusName(record.status),
             text(record.truncationReason), text(record.error))
      .transform([&] {
        return static_cast<std::int64_t>(
            sqlite3_last_insert_rowid(database.nativeHandle()));
      });
}

std::expected<void, std::string>
insertCallGraphRunRows(storage::Database &database, std::int64_t runId,
                       const CallGraphRunRecord &record) {
  for (const auto &[id, usr] : record.roots)
    if (auto row = catalog::execute(
            database, "INSERT INTO callgraph_run_root VALUES(?,?,?)", runId,
            id, usr);
        !row)
      return row;
  if (record.target)
    if (auto row = catalog::execute(
            database, "INSERT INTO callgraph_run_target VALUES(?,?,?)", runId,
            record.target->first, record.target->second);
        !row)
      return row;
  for (const auto &value : record.edges)
    if (auto row = catalog::execute(
            database,
            "INSERT OR IGNORE INTO callgraph_run_edge VALUES(?,?,?,?,?,?,?,?,?)",
            runId, value.edge.source, value.edge.destination, value.edge.kind,
            value.edge.position, value.edge.file, value.edge.offset,
            value.depth, value.cycle ? 1 : 0);
        !row)
      return row;
  for (const auto &item : record.frontier)
    if (auto row = catalog::execute(
            database, "INSERT INTO callgraph_run_frontier VALUES(?,?,?)",
            runId, item.id, item.reason);
        !row)
      return row;
  for (const auto &row : record.recovery)
    if (auto inserted = catalog::execute(
            database, "INSERT INTO callgraph_run_recovery VALUES(?,?,?,?)",
            runId, row.translationUnit, row.outcome, text(row.diagnostic));
        !inserted)
      return inserted;
  return {};
}
} // namespace facts::commands
