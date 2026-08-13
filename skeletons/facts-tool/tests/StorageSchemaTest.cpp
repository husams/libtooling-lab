#include "model/Function.h"
#include "model/Relation.h"
#include "storage/Storage.h"

#include <sqlite3.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <span>
#include <string>
#include <string_view>

namespace {

bool require(bool condition, std::string_view message) {
  if (!condition) {
    std::cerr << message << '\n';
  }
  return condition;
}

bool execute(sqlite3 *database, std::string_view sql) {
  char *message = nullptr;
  const auto result =
      sqlite3_exec(database, sql.data(), nullptr, nullptr, &message);
  if (result == SQLITE_OK) {
    return true;
  }
  std::cerr << (message ? message : sqlite3_errmsg(database)) << '\n';
  sqlite3_free(message);
  return false;
}

std::int64_t scalar(sqlite3 *database, std::string_view sql) {
  sqlite3_stmt *statement = nullptr;
  if (sqlite3_prepare_v2(database, sql.data(), -1, &statement, nullptr) !=
      SQLITE_OK) {
    return -1;
  }
  const auto result = sqlite3_step(statement) == SQLITE_ROW
                          ? sqlite3_column_int64(statement, 0)
                          : -1;
  sqlite3_finalize(statement);
  return result;
}

std::uint64_t packed(facts::SymbolId id) {
  return (static_cast<std::uint64_t>(id.file) << 32U) | id.index;
}

bool noPackedFlags(sqlite3 *database) {
  constexpr std::array tables{"symbol", "parameter", "relation",
                              "template_argument", "template_parameter"};
  return std::ranges::all_of(tables, [&](std::string_view table) {
    return scalar(database, "SELECT COUNT(*) FROM pragma_table_info('" +
                                std::string{table} + "') WHERE name='flags'") ==
           0;
  });
}

bool verifyFreshSchema(const std::string &path) {
  std::filesystem::remove(path);

  const auto symbolFlags =
      static_cast<std::uint32_t>(clang::AS_private) |
      facts::bit(facts::DefinitionBit) | facts::bit(facts::ConstBit) |
      (2U << facts::refQualifierShift) | (2U << facts::constexprShift) |
      facts::bit(facts::NoexceptBit);
  const auto parameterFlags = facts::bit(facts::ParameterBit::PointerBit) |
                              facts::bit(facts::ParameterBit::ConstBit) |
                              facts::bit(facts::ParameterBit::PackBit);

  facts::SymbolId functionId;
  facts::SymbolId destinationId;
  {
    facts::Storage storage{path};
    facts::Function function;
    function.id.file = 1;
    function.usr = "c:@F@queryable";
    function.qualifiedName = "queryable";
    function.loc = {.line = 1, .column = 1, .offset = 0};
    function.flags = symbolFlags;
    function.parameters.push_back({
        .name = "items",
        .type = {.file = 0, .index = 1},
        .loc = {.line = 1, .column = 10, .offset = 9},
        .region = {.offset = 9, .size = 5},
        .flags = parameterFlags,
        .hasDefault = true,
    });

    auto savedFunction = storage.save(function);
    if (!require(savedFunction.has_value(), "failed to save function")) {
      return false;
    }
    functionId = *savedFunction;

    facts::Symbol destination;
    destination.id.file = 1;
    destination.usr = "c:@S@base";
    destination.qualifiedName = "base";
    destination.loc = {.line = 2, .column = 1, .offset = 20};
    auto savedDestination = storage.save(destination);
    if (!require(savedDestination.has_value(), "failed to save destination")) {
      return false;
    }
    destinationId = *savedDestination;

    const auto relationFlags = static_cast<std::uint16_t>(
        clang::AS_protected | facts::bit(facts::VirtualBaseBit) |
        facts::bit(facts::ImplicitEdgeBit) | facts::bit(facts::LexicalBit));
    const std::array relations{facts::Relation{
        .source = functionId,
        .destination = destinationId,
        .kind = facts::RelationKind::Inherits,
        .flags = relationFlags,
    }};
    if (!require(storage.addRelations(relations).has_value(),
                 "failed to save relation")) {
      return false;
    }

    auto loaded = storage.load<facts::Function>(functionId);
    if (!require(loaded.has_value(), "failed to load function") ||
        !require(loaded->flags == symbolFlags,
                 "symbol flags did not round trip") ||
        !require(loaded->parameters.size() == 1,
                 "parameters did not round trip") ||
        !require(loaded->parameters.front().flags == parameterFlags,
                 "parameter flags did not round trip")) {
      return false;
    }
  }

  sqlite3 *database = nullptr;
  if (!require(sqlite3_open(path.c_str(), &database) == SQLITE_OK,
               "failed to open fresh database")) {
    return false;
  }
  const auto functionKey = packed(functionId);
  const auto destinationKey = packed(destinationId);
  const auto valid =
      require(noPackedFlags(database), "fresh schema retained packed flags") &&
      require(scalar(database,
                     "SELECT COUNT(*) FROM symbol WHERE id=" +
                         std::to_string(functionKey) +
                         " AND access='private' AND is_definition=1 AND "
                         "is_const=1 AND ref_qualifier='rvalue' AND "
                         "constant_evaluation='consteval' AND is_noexcept=1") ==
                  1,
              "symbol semantic columns are incorrect") &&
      require(scalar(database,
                     "SELECT COUNT(*) FROM parameter WHERE symbol_id=" +
                         std::to_string(functionKey) +
                         " AND position=0 AND is_pointer=1 AND "
                         "is_lvalue_reference=0 AND is_rvalue_reference=0 AND "
                         "is_forwarding_reference=0 AND is_const=1 AND "
                         "is_pack=1 AND has_default=1") == 1,
              "parameter semantic columns are incorrect") &&
      require(scalar(database,
                     "SELECT COUNT(*) FROM relation WHERE source_id=" +
                         std::to_string(functionKey) + " AND destination_id=" +
                         std::to_string(destinationKey) +
                         " AND access='protected' AND is_virtual_base=1 AND "
                         "is_implicit=1 AND is_lexical=1") == 1,
              "relation semantic columns are incorrect");
  sqlite3_close(database);
  return valid;
}

bool createLegacyDatabase(const std::string &path) {
  std::filesystem::remove(path);
  sqlite3 *database = nullptr;
  if (sqlite3_open(path.c_str(), &database) != SQLITE_OK) {
    return false;
  }
  const auto created = execute(database, R"sql(
CREATE TABLE symbol (
  id INTEGER PRIMARY KEY, file_id INTEGER NOT NULL, file_index INTEGER NOT NULL,
  identity TEXT NOT NULL, node INTEGER NOT NULL, kind INTEGER NOT NULL,
  sub_kind INTEGER NOT NULL, lang INTEGER NOT NULL, properties INTEGER NOT NULL,
  usr TEXT NOT NULL, qualified_name TEXT NOT NULL, line INTEGER NOT NULL,
  col INTEGER NOT NULL, offset INTEGER NOT NULL, flags INTEGER NOT NULL);
CREATE TABLE parameter (
  symbol_id INTEGER NOT NULL, position INTEGER NOT NULL, name TEXT NOT NULL,
  type INTEGER NOT NULL, line INTEGER NOT NULL, col INTEGER NOT NULL,
  offset INTEGER NOT NULL, region_offset INTEGER NOT NULL,
  region_size INTEGER NOT NULL, flags INTEGER NOT NULL,
  has_default INTEGER NOT NULL, PRIMARY KEY(symbol_id,position)) WITHOUT ROWID;
CREATE TABLE relation (
  source_id INTEGER NOT NULL, destination_id INTEGER NOT NULL,
  kind INTEGER NOT NULL, position INTEGER NOT NULL, flags INTEGER NOT NULL,
  count INTEGER NOT NULL,
  PRIMARY KEY(source_id,destination_id,kind,position)) WITHOUT ROWID;
CREATE TABLE template_argument (
  symbol_id INTEGER NOT NULL, position INTEGER NOT NULL, name TEXT NOT NULL,
  type_id INTEGER NOT NULL, flags INTEGER NOT NULL,
  PRIMARY KEY(symbol_id,position)) WITHOUT ROWID;
CREATE TABLE template_parameter (
  symbol_id INTEGER NOT NULL, position INTEGER NOT NULL, value TEXT NOT NULL,
  type_id INTEGER NOT NULL, flags INTEGER NOT NULL, kind INTEGER NOT NULL,
  pack_index INTEGER NOT NULL, PRIMARY KEY(symbol_id,position)) WITHOUT ROWID;
INSERT INTO symbol VALUES(1,1,0,'legacy',1,0,0,0,0,'legacy','legacy',1,1,0,25166918);
INSERT INTO parameter VALUES(1,0,'p',1,1,1,0,0,1,63,0);
INSERT INTO relation VALUES(1,1,2,0,29,1);
INSERT INTO template_argument VALUES(1,0,'T',0,7);
INSERT INTO template_parameter VALUES(1,0,'',0,63,1,-1);
)sql");
  sqlite3_close(database);
  return created;
}

bool verifyMigration(const std::string &path) {
  if (!require(createLegacyDatabase(path),
               "failed to create legacy database")) {
    return false;
  }
  {
    facts::Storage migrated{path};
  }

  sqlite3 *database = nullptr;
  if (!require(sqlite3_open(path.c_str(), &database) == SQLITE_OK,
               "failed to open migrated database")) {
    return false;
  }
  const auto valid =
      require(noPackedFlags(database), "migration retained packed flags") &&
      require(scalar(database,
                     "SELECT COUNT(*) FROM symbol WHERE access='private' AND "
                     "is_definition=1 AND is_const=1 AND "
                     "ref_qualifier='rvalue' AND "
                     "constant_evaluation='consteval' AND is_noexcept=1") == 1,
              "migrated symbol properties are incorrect") &&
      require(
          scalar(database,
                 "SELECT COUNT(*) FROM parameter WHERE is_pointer=1 AND "
                 "is_lvalue_reference=1 AND is_rvalue_reference=1 AND "
                 "is_forwarding_reference=1 AND is_const=1 AND is_pack=1") == 1,
          "migrated parameter properties are incorrect") &&
      require(
          scalar(database,
                 "SELECT COUNT(*) FROM relation WHERE access='protected' "
                 "AND is_virtual_base=1 AND is_implicit=1 AND is_lexical=1") ==
              1,
          "migrated relation properties are incorrect") &&
      require(scalar(database, "SELECT COUNT(*) FROM template_argument WHERE "
                               "is_parameter_pack=1 AND is_non_type=1 AND "
                               "is_template_template=1") == 1,
              "migrated template argument properties are incorrect") &&
      require(scalar(database,
                     "SELECT COUNT(*) FROM template_parameter WHERE "
                     "is_pointer=1 AND is_lvalue_reference=1 AND "
                     "is_rvalue_reference=1 AND is_forwarding_reference=1 "
                     "AND is_const=1 AND is_pack=1") == 1,
              "migrated template parameter properties are incorrect") &&
      require(scalar(database, "PRAGMA user_version") == 1,
              "migration version was not recorded");
  sqlite3_close(database);
  return valid;
}

} // namespace

int main(int argc, char **argv) {
  if (!require(argc == 3,
               "usage: storage-schema-test FRESH_DATABASE LEGACY_DATABASE")) {
    return 1;
  }
  return verifyFreshSchema(argv[1]) && verifyMigration(argv[2]) ? 0 : 1;
}
