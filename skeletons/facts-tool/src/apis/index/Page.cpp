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
          path, row.string(6), cloneName, component.name, row.integer(4) != 0, row.integer(19)};
}
Result<std::vector<Symbol>> rows(Database &database, const Query &query,
                                  const Position &position) {
  constexpr auto sql =
      "SELECT i.qualified_name,i.kind,i.usr,i.file_id,i.is_definition,i.path,"
      "coalesce(r.name,''),c.id,c.name,c.path,c.kind,c.version,c.repository_id,"
      "cl.id,cl.repository_id,cl.path,coalesce(cl.label,''),d.path,f.name,i.position "
      "FROM global_symbol_index i JOIN file f ON f.id=i.file_id "
      "JOIN directory d ON d.id=f.directory_id JOIN component c ON c.id=d.component_id "
      "LEFT JOIN repository r ON r.id=c.repository_id LEFT JOIN clone cl ON cl.id="
      "(SELECT x.id FROM clone x WHERE x.repository_id=r.id AND "
      "((i.path='' AND x.id=r.active_clone_id) OR i.path=x.path OR "
      "substr(i.path,1,length(rtrim(x.path,'/'))+1)=rtrim(x.path,'/')||'/') "
      "ORDER BY length(x.path) DESC,x.id LIMIT 1) "
      "WHERE i.qualified_name=?1 AND (?2 IS NULL OR i.kind=?2) "
      "AND (?3 IS NULL OR i.usr=?3) AND (?4 IS NULL OR r.name=?4) "
      "AND (?5 IS NULL OR c.name=?5) AND i.position>?6 "
      "ORDER BY i.position LIMIT ?7";
  return catalog::query(database, sql, symbol, query.qualifiedName, query.kind,
      query.usr, query.repository, query.component, position.record, query.limit + 1);
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
