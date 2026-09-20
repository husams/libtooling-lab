#include "apis/v2/symbols/Children.h"
#include <array>

namespace facts::apis::v2::symbols {
namespace {
constexpr std::array<std::string_view, 25> relationNames{"unknown", "calls", "inherits", "contains",
    "specializes", "instantiates", "overrides", "uses", "field_of", "method_of",
    "construct_value", "construct_temp", "construct_heap", "construct_copy", "construct_move",
    "factory_construct", "destroy", "friend", "dispatch_calls", "alias_of", "of_type",
    "return_type", "param_type", "template_argument_type", "pointer-call"};
void sqlKey(sqlite3_context *context, int count, sqlite3_value **values) noexcept {
  try {
    Json parts = Json::array();
    for (int i = 0; i < count; ++i) {
      const auto *text = sqlite3_value_text(values[i]);
      parts.push_back(text ? reinterpret_cast<const char *>(text) : "");
    }
    const auto key = digest(parts.dump());
    sqlite3_result_text(context, key.data(), static_cast<int>(key.size()), SQLITE_TRANSIENT);
  } catch (...) { sqlite3_result_error(context, "cannot encode symbol result key", -1); }
}
bool owned(const SourceCatalog &catalog, const index::Symbol &symbol, std::int64_t file) {
  const auto found = catalog.files.find(file);
  if (found == catalog.files.end() || catalog.invalidated.contains(file)) return false;
  const auto &component = found->second.component;
  return symbol.repositoryScope ? component.repositoryId == symbol.scopeId
      : !component.repositoryId && component.id == symbol.scopeId;
}
std::string filePath(const SourceCatalog &catalog, std::int64_t file, const std::string &stored) {
  if (!stored.empty()) return stored;
  const auto found = catalog.files.find(file);
  if (found == catalog.files.end()) return "";
  return catalog::filePath(found->second).transform([](const auto &path) { return path.string(); }).value_or("");
}
Json reference(const SourceCatalog &catalog, std::int64_t file,
               const std::string &usr, const std::string &name) {
  Json id = nullptr;
  if (const auto found = catalog.files.find(file); found != catalog.files.end()) {
    const auto &component = found->second.component;
    id = index::symbolIdentity(usr, component.repositoryId.value_or(component.id),
                               component.repositoryId.has_value());
  }
  return {{"symbol_id", std::move(id)}, {"qualified_name", name}, {"usr", usr}};
}
void retain(ChildPage &page, const std::string &key, Json item) {
  const auto [found, inserted] = page.items.try_emplace(key, std::move(item));
  if (!inserted && item.contains("count"))
    found->second["count"] = std::max(found->second.at("count").get<std::int64_t>(), item.at("count").get<std::int64_t>());
  if (page.items.size() > page.limit + 1) page.items.erase(std::prev(page.items.end()));
}
domain::Result<void> occurrences(catalog::Database &database, const SourceCatalog &catalog,
                                 const StoredSymbol &symbol, ChildPage &page) {
  constexpr auto sql =
      "WITH occurrences AS ("
      "SELECT 'declaration' AS role,s.id>>32 AS file_id,s.line,s.col,s.offset,NULL AS size,"
      "coalesce(p.path,'') AS path,coalesce(d.file_id,s.id>>32) AS owner "
      "FROM symbol s LEFT JOIN definition d ON d.symbol_id=s.id "
      "LEFT JOIN facts_project_provenance p ON p.file_id=s.id>>32 WHERE s.usr=?1 "
      "UNION ALL SELECT 'definition',d.file_id,"
      "CASE WHEN d.file_id=s.id>>32 AND d.offset=s.offset THEN s.line ELSE NULL END,"
      "CASE WHEN d.file_id=s.id>>32 AND d.offset=s.offset THEN s.col ELSE NULL END,"
      "d.offset,d.size,coalesce(p.path,''),d.file_id FROM symbol s "
      "JOIN definition d ON d.symbol_id=s.id "
      "LEFT JOIN facts_project_provenance p ON p.file_id=d.file_id WHERE s.usr=?1),"
      "keyed AS (SELECT *,api_symbol_key(role,file_id,offset,path) AS key FROM occurrences) "
      "SELECT role,file_id,line,col,offset,size,path,owner,key FROM keyed "
      "WHERE key>?2 ORDER BY key LIMIT ?3";
  return catalog::query(database, sql, [](const storage::Row &row) {
    return Json{{"kind", row.string(0)}, {"file_id", std::to_string(row.integer(1))},
        {"line", row.isNull(2) ? Json(nullptr) : Json(row.integer(2))},
        {"column", row.isNull(3) ? Json(nullptr) : Json(row.integer(3))},
        {"offset", row.isNull(4) ? Json(nullptr) : Json(row.integer(4))},
        {"size", row.isNull(5) ? Json(nullptr) : Json(row.integer(5))},
        {"path", row.string(6)}, {"_owner", row.integer(7)}, {"_key", row.string(8)},
        {"_file", row.integer(1)}};
  }, symbol.symbol.usr, page.after, page.limit + 1).transform_error(indexError)
      .transform([&](auto rows) {
        for (auto &row : rows) {
          if (!owned(catalog, symbol.symbol, row.at("_owner"))) continue;
          row["path"] = filePath(catalog, row.at("_file"), row.at("path"));
          const auto key = row.at("_key").template get<std::string>();
          row.erase("_owner"); row.erase("_key"); row.erase("_file");
          retain(page, key, std::move(row));
        }
      });
}
domain::Result<void> relations(catalog::Database &database, const SourceCatalog &catalog,
                               const StoredSymbol &symbol, const QueryParameters &parameters,
                               ChildPage &page) {
  const auto direction = parameters.contains("direction") ? parameters.at("direction") : "outgoing";
  std::optional<unsigned> kind;
  if (parameters.contains("kind")) {
    for (unsigned i = 1; i < relationNames.size(); ++i)
      if (relationNames[i] == parameters.at("kind")) kind = i;
    if (!kind) return std::unexpected(domain::Error{422, "invalid_query", "Unknown relation kind"});
  }
  const std::string predicate = direction == "outgoing" ? "r.source_id=root.id"
      : direction == "incoming" ? "r.destination_id=root.id"
      : "(r.source_id=root.id OR r.destination_id=root.id)";
  const auto sql =
      "WITH keyed AS (SELECT s.usr AS source_usr,s.qualified_name AS source_name,"
      "coalesce(sd.file_id,s.id>>32) AS source_file,t.usr AS target_usr,"
      "t.qualified_name AS target_name,coalesce(td.file_id,t.id>>32) AS target_file,"
      "r.kind,r.position,r.count,coalesce(rd.file_id,root.id>>32) AS owner,"
      "api_symbol_key(s.usr,t.usr,r.kind,r.position) AS key FROM symbol root "
      "JOIN relation r ON " + predicate + " JOIN symbol s ON s.id=r.source_id "
      "JOIN symbol t ON t.id=r.destination_id "
      "LEFT JOIN definition sd ON sd.symbol_id=s.id "
      "LEFT JOIN definition td ON td.symbol_id=t.id "
      "LEFT JOIN definition rd ON rd.symbol_id=root.id "
      "WHERE root.usr=?1 AND (?2 IS NULL OR r.kind=?2)) "
      "SELECT source_usr,source_name,source_file,target_usr,target_name,target_file,"
      "kind,position,count,owner,key FROM keyed WHERE key>?3 ORDER BY key LIMIT ?4";
  return catalog::query(database, sql, [&](const storage::Row &row) {
    const auto rawKind = row.integer(6);
    return Json{{"source", reference(catalog, row.integer(2), row.string(0), row.string(1))},
        {"target", reference(catalog, row.integer(5), row.string(3), row.string(4))},
        {"kind", rawKind > 0 && rawKind < static_cast<std::int64_t>(relationNames.size()) ? relationNames[rawKind] : "unknown"},
        {"position", row.integer(7)}, {"count", row.integer(8)},
        {"_owner", row.integer(9)}, {"_key", row.string(10)}};
  }, symbol.symbol.usr, kind, page.after, page.limit + 1).transform_error(indexError)
      .transform([&](auto rows) {
        for (auto &row : rows) {
          if (!owned(catalog, symbol.symbol, row.at("_owner"))) continue;
          const auto key = row.at("_key").template get<std::string>();
          row.erase("_owner"); row.erase("_key");
          retain(page, key, std::move(row));
        }
      });
}
}
domain::Result<void> collect(catalog::Database &database, const SourceCatalog &catalog,
                            const StoredSymbol &symbol, std::string_view child,
                            const QueryParameters &parameters, ChildPage &page) {
  if (sqlite3_create_function_v2(database.nativeHandle(), "api_symbol_key", -1,
      SQLITE_UTF8 | SQLITE_DETERMINISTIC, nullptr, sqlKey, nullptr, nullptr, nullptr) != SQLITE_OK)
    return std::unexpected(indexError(catalog::databaseError(database)));
  return child == "occurrences" ? occurrences(database, catalog, symbol, page)
                               : relations(database, catalog, symbol, parameters, page);
}
}
