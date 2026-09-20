#include "apis/index/Internal.h"
#include "storage/ProjectConfiguration.h"

namespace facts::apis::index {
namespace {
Symbol symbol(const storage::Row &row) {
  ProjectComponent component{row.integer(7), row.string(8), row.string(9),
      row.string(10), row.get<std::optional<std::string>>(11),
      row.get<std::optional<std::int64_t>>(12)};
  std::optional<ProjectClone> clone;
  if (!row.isNull(13))
    clone = ProjectClone{row.integer(13), row.integer(14), row.string(15), row.string(16)};
  auto path = row.string(5);
  if (path.empty())
    path = fullProjectFilePath(component, clone, row.string(17), row.string(18)).string();
  const auto cloneName = clone ? (clone->label.empty() ? std::to_string(clone->id) : clone->label) : "";
  return {row.string(0), row.string(1), row.string(2), row.integer(3),
          path, row.string(6), cloneName, component.name, row.integer(4) != 0, row.integer(19),
          row.string(20), component.repositoryId.value_or(component.id),
          component.repositoryId.has_value()};
}
Result<std::vector<Symbol>> rows(Database &database, const Query &query,
                                  const Position &position) {
  const bool prefix = query.match == NameMatch::Prefix && !query.qualifiedName.empty();
  std::string sql =
      "SELECT i.qualified_name,i.kind,i.usr,i.file_id,i.is_definition,i.path,"
      "coalesce(r.name,''),c.id,c.name,c.path,c.kind,c.version,c.repository_id,"
      "cl.id,cl.repository_id,cl.path,coalesce(cl.label,''),d.path,f.name,i.position,i.symbol_id "
      "FROM global_symbol_index i INDEXED BY " +
      std::string(query.symbolId ? "global_symbol_id" : query.usr ? "global_symbol_usr" : "global_symbol_name") +
      " JOIN file f ON f.id=i.file_id "
      "JOIN directory d ON d.id=f.directory_id JOIN component c ON c.id=d.component_id "
      "LEFT JOIN repository r ON r.id=c.repository_id LEFT JOIN clone cl ON cl.id="
      "(SELECT x.id FROM clone x WHERE x.repository_id=r.id AND "
      "((i.path='' AND x.id=r.active_clone_id) OR i.path=x.path OR "
      "substr(i.path,1,length(rtrim(x.path,'/'))+1)=rtrim(x.path,'/')||'/') "
      "ORDER BY length(x.path) DESC,x.id LIMIT 1) "
      "WHERE ";
  std::string upper = query.qualifiedName;
  while (!upper.empty() && static_cast<unsigned char>(upper.back()) == 255) upper.pop_back();
  if (!upper.empty()) upper.back() = static_cast<char>(static_cast<unsigned char>(upper.back()) + 1);
  if (query.qualifiedName.empty()) sql += "?1=''";
  else if (query.match == NameMatch::Exact) sql += "i.qualified_name=?1";
  else sql += "i.qualified_name>=?1 AND i.qualified_name<?8";
  sql += " AND (?2 IS NULL OR i.kind=?2) ";
  sql += query.usr ? "AND i.usr=?3 " : "AND ?3 IS NULL ";
  sql +=
      "AND (?4 IS NULL OR r.name=?4) AND (?5 IS NULL OR c.name=?5) ";
  sql += prefix && position.record > 0
      ? "AND (i.qualified_name,i.position)>((SELECT qualified_name FROM global_symbol_index WHERE position=?6),?6) "
      : "AND i.position>?6 ";
  sql += "AND NOT EXISTS (SELECT 1 FROM api_index_invalidated_file invalid WHERE invalid.file_id=i.file_id) ";
  sql += query.symbolId ? "AND i.symbol_id=?9 " : "AND ?9 IS NULL ";
  if (query.distinct)
    sql += "AND i.position=(SELECT chosen.position FROM global_symbol_index chosen "
        "WHERE chosen.symbol_id=i.symbol_id AND NOT EXISTS "
        "(SELECT 1 FROM api_index_invalidated_file invalid WHERE invalid.file_id=chosen.file_id) "
        "ORDER BY chosen.is_definition DESC,chosen.position LIMIT 1) ";
  sql += prefix ? "ORDER BY i.qualified_name,i.position LIMIT ?7" : "ORDER BY i.position LIMIT ?7";
  return catalog::query(database, sql, symbol, query.qualifiedName, query.kind,
      query.usr, query.repository, query.component, position.record, query.limit + 1,
      upper, query.symbolId);
}
}
Result<Page> readPage(Database &database, const Query &query,
                      const Position &position) {
  return catalog::query(database,
      "SELECT generation FROM global_symbol_index_state WHERE id=1",
      [](const storage::Row &row) { return row.integer(0); })
      .and_then([](auto values) { return catalog::requireOne(std::move(values), "symbol index state"); })
      .and_then([&](std::int64_t generation) -> Result<Page> {
        if (generation == 0) return std::unexpected("symbol index is not ready");
        if (query.cursor && generation != position.generation)
          return std::unexpected("symbol index changed; restart pagination without a cursor");
        return rows(database, query, position).transform([&](auto values) {
          Page page{std::move(values), std::nullopt, generation};
          if (page.items.size() > query.limit) {
            page.items.pop_back();
            const auto &last = page.items.back();
            page.nextCursor = encodeCursor(query, {generation, last.position});
          }
          return page;
        });
      });
}
}
