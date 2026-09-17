#include "commands/analyse/CallGraphRunRows.h"

#include "storage/catalog/Database.h"

namespace facts::commands {

std::expected<void, std::string>
insertCallGraphRunPointerRows(
    storage::Database &database, std::int64_t runId,
    const std::vector<callgraph::QueryPointerCall> &calls) {
  for (const auto &call : calls) {
    const auto &site = call.site;
    if (auto row = catalog::execute(
            database,
            "INSERT INTO callgraph_run_pointer_call_site("
            "run_id,source_id,target_id,file_id,offset,line,col,signature,"
            "expression,target_name,target_usr) VALUES(?,?,?,?,?,?,?,?,?,?,?)",
            runId, site.source, site.target, site.file, site.location.offset,
            site.location.line, site.location.column, site.signature,
            site.expression,
            call.target.transform([](const auto &target) { return target.name; }),
            call.target.transform([](const auto &target) { return target.usr; }));
        !row)
      return row;
  }
  return {};
}

} // namespace facts::commands
