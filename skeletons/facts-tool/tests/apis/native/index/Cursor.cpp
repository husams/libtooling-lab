#include "Fixture.h"

namespace index_test {
void verifyLongCursor() {
  Fixture fixture;
  const std::string usr(20000, 'T');
  auto first = facts::storage::Database::open(fixture.first.string(),
      facts::storage::Database::readWrite);
  auto second = facts::storage::Database::open(fixture.second.string(),
      facts::storage::Database::readWrite);
  assert(first && second);
  for (auto *database : {&*first, &*second})
    assert(facts::catalog::execute(*database,
        "UPDATE symbol SET usr=? WHERE qualified_name='shared::same'", usr));
  assert(index::refresh(fixture.project));
  index::Query query{.qualifiedName = "shared::same", .limit = 1};
  auto page = index::search(fixture.project, query);
  assert(page && page->items.size() == 1 && page->nextCursor);
  assert(page->items[0].usr == usr && page->nextCursor->size() < 256);
  query.cursor = page->nextCursor;
  auto next = index::search(fixture.project, query);
  assert(next && next->items.size() == 1 && !next->nextCursor);
  assert(next->items[0].usr == usr && next->items[0].fileId != page->items[0].fileId);
}
}

namespace index_test {
void verifyPrefix() {
  Fixture fixture;
  execute(fixture.first, "DELETE FROM symbol; DELETE FROM definition");
  execute(fixture.second, "DELETE FROM symbol; DELETE FROM definition");
  auto database = facts::storage::Database::open(fixture.first.string(), facts::storage::Database::readWrite);
  assert(database);
  const std::vector<std::string> names{"example::Widget", "example::Widget::run", "example::widget", "example::Wid_get", "example::WidXget", "example::Wid%get", "example::WidZget", "example::Widget0"};
  for (std::size_t i = 0; i < names.size(); ++i)
    assert(facts::catalog::execute(*database,
        "INSERT INTO symbol VALUES(?,?,?,13,1)", 4294967300LL + i,
        "symbol-" + std::to_string(i), names[i]));
  assert(index::refresh(fixture.project));
  index::Query query{.qualifiedName = "example::Widget"};
  auto exact = index::search(fixture.project, query);
  assert(exact && exact->items.size() == 1); // v1 retains exact matching.
  query.match = index::NameMatch::Prefix;
  query.limit = 1;
  query.distinct = true;
  auto prefix = index::search(fixture.project, query);
  assert(prefix && prefix->items.size() == 1 && prefix->nextCursor);
  const auto symbolId = prefix->items.front().symbolId;
  assert(symbolId.starts_with("sym_") && symbolId.size() == 68);
  query.cursor = prefix->nextCursor;
  auto next = index::search(fixture.project, query);
  assert(next && next->items.size() == 1 && next->nextCursor);
  assert(next->items[0].qualifiedName == "example::Widget0");
  query.cursor = next->nextCursor;
  auto last = index::search(fixture.project, query);
  assert(last && last->items.size() == 1 && !last->nextCursor);
  assert(last->items[0].qualifiedName == "example::Widget::run");
  query.match = index::NameMatch::Exact;
  assert(!index::search(fixture.project, query));
  query.match = index::NameMatch::Prefix;
  query.repository = "alpha";
  assert(!index::search(fixture.project, query));
  query.cursor.reset(); query.repository.reset(); query.limit = 50;
  for (const auto &literal : {"example::Wid_", "example::Wid%"}) {
    query.qualifiedName = literal;
    auto result = index::search(fixture.project, query);
    assert(result && result->items.size() == 1);
    assert(result->items[0].qualifiedName.starts_with(literal));
  }
  assert(index::refresh(fixture.project));
  index::Query byId;
  byId.symbolId = symbolId;
  auto stable = index::search(fixture.project, byId);
  assert(stable && stable->items.size() == 1 && stable->items[0].qualifiedName == names[0]);
  index::Query byUsr;
  byUsr.usr = "symbol-0";
  auto usr = index::search(fixture.project, byUsr);
  assert(usr && usr->items.size() == 1);
  auto project = facts::catalog::open(fixture.project.string(), false);
  assert(project);
  auto plan = facts::catalog::query(*project,
      "EXPLAIN QUERY PLAN SELECT usr,file_id FROM global_symbol_index "
      "WHERE qualified_name>='example::Wid' AND qualified_name<'example::Wie' ORDER BY qualified_name,position",
      [](const facts::storage::Row &row) { return row.string(3); });
  assert(plan);
  bool indexed = false;
  for (const auto &detail : *plan) {
    indexed |= detail.find("USING COVERING INDEX global_symbol_name") != std::string::npos;
    assert(detail.find("TEMP B-TREE") == std::string::npos);
  }
  assert(indexed);
}
}

#include "apis/v2/symbols/Symbols.h"
namespace index_test {
void verifySymbolResources() {
  Fixture fixture;
  for (const auto &path : {fixture.first, fixture.second})
    execute(path, R"sql(
ALTER TABLE symbol ADD COLUMN line INTEGER NOT NULL DEFAULT 12;
ALTER TABLE symbol ADD COLUMN col INTEGER NOT NULL DEFAULT 7;
ALTER TABLE symbol ADD COLUMN offset INTEGER NOT NULL DEFAULT 100;
ALTER TABLE definition ADD COLUMN offset INTEGER NOT NULL DEFAULT 200;
ALTER TABLE definition ADD COLUMN size INTEGER NOT NULL DEFAULT 30;
CREATE TABLE relation(source_id INTEGER,destination_id INTEGER,kind INTEGER,position INTEGER,count INTEGER);
)sql");
  execute(fixture.first, "INSERT INTO relation VALUES(4294967297,12884901889,1,0,2)");
  assert(index::refresh(fixture.project));
  facts::apis::domain::Context context;
  context.configuration.database = fixture.project;
  context.configuration.projectRoot = fixture.root;
  namespace symbols = facts::apis::v2::symbols;
  const auto search = symbols::search(context, {{"qualified_name", "shared::"}});
  assert(search && search->at("items").size() == 2);
  const auto &symbol = search->at("items").at(0);
  const auto id = symbol.at("symbol_id").get<std::string>();
  auto detail = symbols::read(context, id, "", {});
  assert(detail && *detail == symbol);
  auto occurrences = symbols::read(context, id, "occurrences", {});
  assert(occurrences && occurrences->at("items").size() == 1);
  assert(occurrences->at("items").at(0).at("line") == 12);
  assert(occurrences->at("items").at(0).at("file_id") == "1");
  auto relations = symbols::read(context, id, "relations", {{"kind", "calls"}});
  assert(relations && relations->at("items").size() == 1);
  assert(relations->at("items").at(0).at("count") == 2);
  assert(relations->at("items").at(0).at("target").at("qualified_name") == "alpha::defined");
  auto defined = symbols::search(context, {{"qualified_name", "alpha::defined"}});
  assert(defined && defined->at("items").size() == 1);
  const auto definedId = defined->at("items").at(0).at("symbol_id").get<std::string>();
  auto page = symbols::read(context, definedId, "occurrences", {{"limit", "1"}});
  assert(page && page->at("items").size() == 1 && page->at("next_cursor").is_string());
  const auto cursor = page->at("next_cursor").get<std::string>();
  auto next = symbols::read(context, definedId, "occurrences", {{"limit", "1"}, {"cursor", cursor}});
  assert(next && next->at("items").size() == 1 && next->at("next_cursor").is_null());
  assert(next->at("items").at(0) != page->at("items").at(0));
  assert(!symbols::read(context, id, "occurrences", {{"cursor", cursor}}));
  execute(fixture.first, "UPDATE symbol SET line=13");
  assert(!symbols::read(context, definedId, "occurrences", {{"cursor", cursor}}));
  assert(!symbols::search(context, {{"qualified_name", "shared"}, {"match", "contains"}}));
  assert(!symbols::read(context, id, "relations", {{"kind", "unknown"}}));
  assert(!symbols::read(context, "sym_missing", "", {}));
}
}

namespace index_test {
void verifyInvalidatedSymbols() {
  Fixture fixture;
  assert(index::refresh(fixture.project));
  execute(fixture.project, "INSERT INTO api_index_invalidated_file VALUES(1)");
  assert(index::refresh(fixture.project));
  auto hidden = index::search(fixture.project, {.qualifiedName = "shared::same"});
  assert(hidden && hidden->items.size() == 1 && hidden->items[0].repository == "beta");
  execute(fixture.project, "UPDATE file SET indexed=1 WHERE id=1");
  assert(index::refresh(fixture.project));
  auto restored = index::search(fixture.project, {.qualifiedName = "shared::same"});
  assert(restored && restored->items.size() == 2);
}
}
