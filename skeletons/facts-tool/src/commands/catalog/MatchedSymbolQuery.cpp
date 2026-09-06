#include "commands/catalog/MatchedSymbolQuery.h"

#include "storage/ProjectConfiguration.h"

namespace facts::commands {

catalog::Result<std::vector<MatchedSymbolCandidate>>
findMatchedSymbols(catalog::Database &database,
                   const std::optional<std::string> &usr,
                   const std::optional<std::string> &name,
                   const std::optional<std::int64_t> &kind) {
  constexpr auto sql =
      "SELECT m.usr,m.qualified_name,m.file_id,m.kind,c.id,c.name,c.path,"
      "c.kind,c.version,c.repository_id,cl.id,cl.repository_id,cl.path,"
      "cl.label,d.path,f.name,coalesce(r.name,'') "
      "FROM matched_symbol_index m JOIN file f ON f.id=m.file_id "
      "JOIN directory d ON d.id=f.directory_id "
      "JOIN component c ON c.id=d.component_id "
      "LEFT JOIN repository r ON r.id=c.repository_id "
      "LEFT JOIN clone cl ON cl.id=r.active_clone_id "
      "WHERE (?1 IS NULL OR m.usr=?1) "
      "AND (?2 IS NULL OR instr(m.qualified_name,?2)>0) "
      "AND (?3 IS NULL OR m.kind=?3) ORDER BY m.usr,m.file_id";
  return catalog::query(
      database, sql,
      [](const storage::Row &row) {
        ProjectComponent component{row.integer(4),
                                   row.string(5),
                                   row.string(6),
                                   row.string(7),
                                   row.get<std::optional<std::string>>(8),
                                   row.get<std::optional<std::int64_t>>(9)};
        std::optional<ProjectClone> clone;
        if (!row.isNull(10))
          clone = ProjectClone{row.integer(10), row.integer(11), row.string(12),
                               row.string(13)};
        return MatchedSymbolCandidate{
            {row.string(0), row.string(1), row.get<FileId>(2), row.integer(3)},
            fullProjectFilePath(component, clone, row.string(14),
                                row.string(15)),
            component.name,
            row.string(16)};
      },
      usr, name, kind);
}

catalog::Result<std::size_t> clearMatchedSymbols(catalog::Database &database,
                                                 FileId fileId) {
  return catalog::query(
             database,
             "DELETE FROM matched_symbol_index WHERE file_id=?1 "
             "RETURNING file_id",
             [](const storage::Row &) { return 1U; }, fileId)
      .transform([](const auto &rows) { return rows.size(); });
}

} // namespace facts::commands
