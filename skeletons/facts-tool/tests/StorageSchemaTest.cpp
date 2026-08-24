#include "model/Function.h"
#include "model/RecordInstance.h"
#include "model/RecordTemplate.h"
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
#include <vector>

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

std::string textScalar(sqlite3 *database, std::string_view sql) {
  sqlite3_stmt *statement = nullptr;
  if (sqlite3_prepare_v2(database, sql.data(), -1, &statement, nullptr) !=
      SQLITE_OK) {
    return {};
  }
  std::string value;
  if (sqlite3_step(statement) == SQLITE_ROW) {
    const auto *text = sqlite3_column_text(statement, 0);
    value = text ? reinterpret_cast<const char *>(text) : std::string{};
  }
  sqlite3_finalize(statement);
  return value;
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

bool noRedundantSymbolIdColumns(sqlite3 *database) {
  return scalar(database,
                "SELECT COUNT(*) FROM pragma_table_info('symbol') WHERE "
                "name IN ('file_id','file_index')") == 0;
}

bool usrIsOnlySymbolIdentity(sqlite3 *database) {
  return scalar(database,
                "SELECT COUNT(*) FROM pragma_table_info('symbol') WHERE "
                "name='identity'") == 0;
}

bool verifyFreshSchema(const std::string &path) {
  std::filesystem::remove(path);

  const auto symbolFlags =
      static_cast<std::uint32_t>(clang::AS_private) |
      facts::bit(facts::DefinitionBit) | facts::bit(facts::ConstBit) |
      facts::bit(facts::ExternStorageBit) | (2U << facts::refQualifierShift) |
      (2U << facts::constexprShift) | facts::bit(facts::NoexceptBit);
  const auto parameterFlags = facts::bit(facts::ParameterBit::PointerBit) |
                              facts::bit(facts::ParameterBit::ConstBit) |
                              facts::bit(facts::ParameterBit::PackBit);

  facts::SymbolId functionId;
  facts::SymbolId destinationId;
  facts::SymbolId enumerationId;
  facts::SymbolId enumeratorId;
  facts::SymbolId variableId;
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
        .defaultValue =
            facts::Initializer{
                .expression = "2 + 3",
                .evaluated =
                    facts::EvaluatedValue{
                        .kind = facts::EvaluatedValueKind::Integer,
                        .value = "5",
                    },
            },
    });
    function.parameters.push_back({
        .name = "removed",
        .type = {.file = 0, .index = 2},
        .loc = {.line = 1, .column = 20, .offset = 19},
        .region = {.offset = 19, .size = 7},
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

    auto duplicateDestination = destination;
    duplicateDestination.id.file = 2;
    auto savedDuplicate = storage.save(duplicateDestination);
    if (!require(savedDuplicate.has_value() && *savedDuplicate == destinationId,
                 "USR did not preserve symbol identity across files")) {
      return false;
    }

    facts::Symbol missingUsr;
    missingUsr.id.file = 1;
    auto rejected = storage.save(missingUsr);
    if (!require(!rejected &&
                     rejected.error() ==
                         std::make_error_code(std::errc::invalid_argument),
                 "symbol without a USR was accepted")) {
      return false;
    }

    facts::Variable variable;
    variable.id.file = 1;
    variable.usr = "c:@V@initializer";
    variable.qualifiedName = "initializer";
    variable.loc = {.line = 3, .column = 1, .offset = 30};
    variable.flags = facts::bit(facts::DefinitionBit);
    variable.definition = facts::Region{30, 12};
    variable.initializer = facts::Initializer{
        .expression = "2 + 3",
        .evaluated =
            facts::EvaluatedValue{
                .kind = facts::EvaluatedValueKind::Integer,
                .value = "5",
            },
    };
    auto savedVariable = storage.save(variable);
    if (!require(savedVariable.has_value(), "failed to save variable")) {
      return false;
    }
    variableId = *savedVariable;

    facts::Enumeration enumeration;
    enumeration.id.file = 1;
    enumeration.usr = "c:@E@Mode";
    enumeration.qualifiedName = "Mode";
    enumeration.loc = {.line = 4, .column = 1, .offset = 50};
    enumeration.flags = facts::bit(facts::DefinitionBit);
    enumeration.definition = facts::Region{50, 30};
    enumeration.underlyingType = {.file = 0, .index = 7};
    enumeration.isScoped = true;
    enumeration.hasFixedUnderlyingType = true;
    auto savedEnumeration = storage.save(enumeration);
    if (!require(savedEnumeration.has_value(), "failed to save enumeration")) {
      return false;
    }
    enumerationId = *savedEnumeration;

    facts::Enumerator enumerator;
    enumerator.id.file = 1;
    enumerator.usr = "c:@E@Mode@Fast";
    enumerator.qualifiedName = "Mode::Fast";
    enumerator.loc = {.line = 4, .column = 20, .offset = 69};
    enumerator.flags = facts::bit(facts::DefinitionBit);
    enumerator.definition = facts::Region{69, 8};
    enumerator.value = "5";
    enumerator.initializerExpression = "5";
    auto savedEnumerator = storage.save(enumerator);
    if (!require(savedEnumerator.has_value(), "failed to save enumerator")) {
      return false;
    }
    enumeratorId = *savedEnumerator;

    facts::RecordTemplate recordTemplate;
    recordTemplate.id.file = 1;
    recordTemplate.usr = "c:@S@BoxTemplate";
    recordTemplate.qualifiedName = "Box";
    recordTemplate.loc = {.line = 5, .column = 1, .offset = 80};
    recordTemplate.flags = facts::bit(facts::DefinitionBit);
    recordTemplate.definition = facts::Region{80, 20};
    recordTemplate.templateArguments.push_back({.name = "T"});
    if (!require(storage.save(recordTemplate).has_value(),
                 "failed to save record template")) {
      return false;
    }

    facts::RecordInstance recordInstance;
    recordInstance.id.file = 1;
    recordInstance.usr = "c:@S@BoxInt";
    recordInstance.qualifiedName = "Box<int *>";
    recordInstance.loc = {.line = 6, .column = 1, .offset = 100};
    recordInstance.flags = facts::bit(facts::DefinitionBit);
    recordInstance.definition = facts::Region{100, 24};
    recordInstance.templateParameters.push_back({
        .type = {.file = 0, .index = 8},
        .flags = facts::bit(facts::ParameterBit::PointerBit),
    });
    if (!require(storage.save(recordInstance).has_value(),
                 "failed to save record instance")) {
      return false;
    }

    const auto relationFlags = static_cast<std::uint16_t>(
        clang::AS_protected | facts::bit(facts::VirtualBaseBit) |
        facts::bit(facts::ImplicitEdgeBit) | facts::bit(facts::LexicalBit));
    const std::array rejectedRelations{
        facts::Relation{
            .source = functionId,
            .destination = destinationId,
            .kind = facts::RelationKind::Calls,
        },
        facts::Relation{
            .source = functionId,
            .destination = {.file = 99, .index = 1},
            .kind = facts::RelationKind::Calls,
            .position = 1,
        },
    };
    if (!require(!storage.addRelations(rejectedRelations).has_value(),
                 "failing relation batch was accepted")) {
      return false;
    }
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

    const std::array enumRelations{facts::Relation{
        .source = enumerationId,
        .destination = enumeratorId,
        .kind = facts::RelationKind::Contains,
        .flags = static_cast<std::uint16_t>(facts::bit(facts::LexicalBit)),
    }};
    if (!require(storage.addRelations(enumRelations).has_value(),
                 "failed to save enum ownership")) {
      return false;
    }

    auto loaded = storage.load<facts::Function>(functionId);
    if (!require(loaded.has_value(), "failed to load function") ||
        !require(loaded->flags == symbolFlags,
                 "symbol flags did not round trip") ||
        !require(loaded->parameters.size() == 2,
                 "parameters did not round trip") ||
        !require(loaded->parameters.front().flags == parameterFlags,
                 "parameter flags did not round trip") ||
        !require(loaded->parameters.front().defaultValue.has_value(),
                 "parameter default did not round trip") ||
        !require(loaded->parameters.front().defaultValue->expression == "2 + 3",
                 "parameter default expression did not round trip") ||
        !require(
            loaded->parameters.front().defaultValue->evaluated &&
                loaded->parameters.front().defaultValue->evaluated->value ==
                    "5",
            "parameter evaluated default did not round trip")) {
      return false;
    }

    function.qualifiedName = "queryableUpdated";
    function.parameters.front().name = "values";
    function.parameters.resize(1);
    auto updatedFunction = storage.save(function);
    auto loadedByUsr = storage.load<facts::Function>(function.usr);
    auto missing =
        storage.load<facts::Function>(facts::SymbolId{.file = 77, .index = 1});
    if (!require(updatedFunction == savedFunction,
                 "function update changed symbol identity") ||
        !require(loadedByUsr && loadedByUsr->has_value(),
                 "updated function was not found by USR") ||
        !require((*loadedByUsr)->qualifiedName == "queryableUpdated",
                 "symbol update did not round trip") ||
        !require((*loadedByUsr)->parameters.size() == 1 &&
                     (*loadedByUsr)->parameters.front().name == "values",
                 "parameter deletion did not round trip") ||
        !require(!missing && missing.error() ==
                                 std::make_error_code(
                                     std::errc::no_such_file_or_directory),
                 "missing symbol did not report the expected error")) {
      return false;
    }

    auto loadedEnumeration = storage.load<facts::Enumeration>(enumerationId);
    auto loadedEnumerator = storage.load<facts::Enumerator>(enumeratorId);
    if (!require(loadedEnumeration.has_value(), "failed to load enumeration") ||
        !require(loadedEnumeration->underlyingType ==
                     enumeration.underlyingType,
                 "enumeration underlying type did not round trip") ||
        !require(loadedEnumeration->isScoped &&
                     loadedEnumeration->hasFixedUnderlyingType,
                 "enumeration flags did not round trip") ||
        !require(loadedEnumerator.has_value(), "failed to load enumerator") ||
        !require(loadedEnumerator->value == "5",
                 "enumerator value did not round trip") ||
        !require(loadedEnumerator->initializerExpression == "5",
                 "enumerator initializer did not round trip")) {
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
  const auto canonical = textScalar(database, R"sql(
SELECT group_concat(record, char(10)) FROM (
  SELECT 'definition|' || symbol.usr || '|' || definition.file_id || '|' ||
         definition.offset || '|' || definition.size AS record
  FROM definition JOIN symbol ON symbol.id=definition.symbol_id
  UNION ALL
  SELECT 'enumeration|' || symbol.usr || '|' || enumeration.underlying_type ||
         '|' || enumeration.is_scoped || '|' ||
         enumeration.has_fixed_underlying_type AS record
  FROM enumeration JOIN symbol ON symbol.id=enumeration.symbol_id
  UNION ALL
  SELECT 'enumerator|' || symbol.usr || '|' || enumerator.value || '|' ||
         enumerator.initializer_expression
  FROM enumerator JOIN symbol ON symbol.id=enumerator.symbol_id
  UNION ALL
  SELECT 'parameter|' || symbol.usr || '|' || parameter.position || '|' ||
         parameter.name || '|' || parameter.has_default || '|' ||
         COALESCE(parameter_default.expression,'') || '|' ||
         COALESCE(parameter_default.evaluated_kind,'') || '|' ||
         COALESCE(parameter_default.evaluated_value,'')
  FROM parameter JOIN symbol ON symbol.id=parameter.symbol_id
  LEFT JOIN parameter_default USING(symbol_id,position)
  UNION ALL
  SELECT 'relation|' || source.usr || '|' || destination.usr
  FROM relation
  JOIN symbol source ON source.id=relation.source_id
  JOIN symbol destination ON destination.id=relation.destination_id
  UNION ALL
  SELECT 'symbol|' || usr || '|' || qualified_name FROM symbol
  UNION ALL
  SELECT 'template_argument|' || symbol.usr || '|' ||
         template_argument.position || '|' || template_argument.name || '|' ||
         template_argument.type_id || '|' ||
         template_argument.is_parameter_pack || '|' ||
         template_argument.is_non_type || '|' ||
         template_argument.is_template_template
  FROM template_argument
  JOIN symbol ON symbol.id=template_argument.symbol_id
  UNION ALL
  SELECT 'template_parameter|' || symbol.usr || '|' ||
         template_parameter.position || '|' || template_parameter.value ||
         '|' || template_parameter.type_id || '|' ||
         template_parameter.is_pointer || '|' ||
         template_parameter.is_lvalue_reference || '|' ||
         template_parameter.is_rvalue_reference || '|' ||
         template_parameter.is_forwarding_reference || '|' ||
         template_parameter.is_const || '|' || template_parameter.is_pack ||
         '|' || template_parameter.kind || '|' || template_parameter.pack_index
  FROM template_parameter
  JOIN symbol ON symbol.id=template_parameter.symbol_id
  UNION ALL
  SELECT 'variable|' || symbol.usr || '|' || variable_initializer.expression ||
         '|' || variable_initializer.evaluated_kind || '|' ||
         variable_initializer.evaluated_value
  FROM variable_initializer
  JOIN symbol ON symbol.id=variable_initializer.symbol_id
  ORDER BY record
)
)sql");
  constexpr std::string_view canonicalBaseline =
      "definition|c:@E@Mode@Fast|1|69|8\n"
      "definition|c:@E@Mode|1|50|30\n"
      "definition|c:@S@BoxInt|1|100|24\n"
      "definition|c:@S@BoxTemplate|1|80|20\n"
      "definition|c:@V@initializer|1|30|12\n"
      "enumeration|c:@E@Mode|7|1|1\n"
      "enumerator|c:@E@Mode@Fast|5|5\n"
      "parameter|c:@F@queryable|0|values|1|2 + 3|integer|5\n"
      "relation|c:@E@Mode|c:@E@Mode@Fast\n"
      "relation|c:@F@queryable|c:@S@base\n"
      "symbol|c:@E@Mode@Fast|Mode::Fast\n"
      "symbol|c:@E@Mode|Mode\n"
      "symbol|c:@F@queryable|queryableUpdated\n"
      "symbol|c:@S@BoxInt|Box<int *>\n"
      "symbol|c:@S@BoxTemplate|Box\n"
      "symbol|c:@S@base|base\n"
      "symbol|c:@V@initializer|initializer\n"
      "template_argument|c:@S@BoxTemplate|0|T|0|0|0|0\n"
      "template_parameter|c:@S@BoxInt|0||8|1|0|0|0|0|0|1|-1\n"
      "variable|c:@V@initializer|2 + 3|integer|5";
  const auto valid =
      require(canonical == canonicalBaseline,
              "canonical persistence dump changed") &&
      require(scalar(database, "SELECT COUNT(*) FROM relation WHERE kind=" +
                                   std::to_string(static_cast<int>(
                                       facts::RelationKind::Calls))) == 0,
              "failing relation batch was not rolled back") &&
      require(noPackedFlags(database), "fresh schema retained packed flags") &&
      require(noRedundantSymbolIdColumns(database),
              "fresh schema retained redundant symbol id columns") &&
      require(usrIsOnlySymbolIdentity(database),
              "fresh schema retained a separate symbol identity") &&
      require(scalar(database,
                     "SELECT COUNT(*) FROM symbol WHERE id=" +
                         std::to_string(functionKey) +
                         " AND access='private' AND is_definition=1 AND "
                         "is_const=1 AND ref_qualifier='rvalue' AND "
                         "has_extern_storage=1 AND "
                         "constant_evaluation='consteval' AND is_noexcept=1") ==
                  1,
              "symbol semantic columns are incorrect") &&
      require(scalar(database,
                     "SELECT COUNT(*) FROM variable_initializer WHERE "
                     "symbol_id=" +
                         std::to_string(packed(variableId)) +
                         " AND expression='2 + 3' AND "
                         "evaluated_kind='integer' AND evaluated_value='5'") ==
                  1,
              "variable initializer columns are incorrect") &&
      require(
          scalar(database, "SELECT COUNT(*) FROM enumeration WHERE symbol_id=" +
                               std::to_string(packed(enumerationId)) +
                               " AND underlying_type=" +
                               std::to_string(packed({.file = 0, .index = 7})) +
                               " AND is_scoped=1 AND "
                               "has_fixed_underlying_type=1") == 1,
          "enumeration columns are incorrect") &&
      require(scalar(database,
                     "SELECT COUNT(*) FROM enumerator WHERE symbol_id=" +
                         std::to_string(packed(enumeratorId)) +
                         " AND value='5' AND initializer_expression='5'") == 1,
              "enumerator columns are incorrect") &&
      require(scalar(database,
                     "SELECT COUNT(*) FROM parameter WHERE symbol_id=" +
                         std::to_string(functionKey) +
                         " AND position=0 AND is_pointer=1 AND "
                         "is_lvalue_reference=0 AND is_rvalue_reference=0 AND "
                         "is_forwarding_reference=0 AND is_const=1 AND "
                         "is_pack=1 AND has_default=1") == 1,
              "parameter semantic columns are incorrect") &&
      require(scalar(database,
                     "SELECT COUNT(*) FROM parameter_default WHERE symbol_id=" +
                         std::to_string(functionKey) +
                         " AND position=0 AND expression='2 + 3' AND "
                         "evaluated_kind='integer' AND evaluated_value='5'") ==
                  1,
              "parameter default columns are incorrect") &&
      require(scalar(database,
                     "SELECT COUNT(*) FROM relation WHERE source_id=" +
                         std::to_string(functionKey) + " AND destination_id=" +
                         std::to_string(destinationKey) +
                         " AND access='protected' AND is_virtual_base=1 AND "
                         "is_implicit=1 AND is_lexical=1") == 1,
              "relation semantic columns are incorrect") &&
      require(scalar(database, "SELECT COUNT(*) FROM pragma_table_info("
                               "'relation_site')") == 8,
              "fresh relation-site schema is incomplete") &&
      require(scalar(database, "SELECT COUNT(*) FROM pragma_foreign_key_list("
                               "'relation_site') WHERE [table]='relation' "
                               "AND on_delete='CASCADE'") == 4,
              "relation-site cascade key is incorrect") &&
      require(scalar(database, "PRAGMA user_version") == 7,
              "fresh schema version was not recorded");
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
INSERT INTO symbol VALUES(1,1,0,'legacy',1,0,0,0,0,'legacy','legacy',1,1,0,27264070);
INSERT INTO parameter VALUES(1,0,'p',1,1,1,0,0,1,63,0);
INSERT INTO relation VALUES(1,1,2,0,29,1);
INSERT INTO template_argument VALUES(1,0,'T',0,7);
INSERT INTO template_parameter VALUES(1,0,'',0,63,1,-1);
)sql");
  sqlite3_close(database);
  return created;
}

bool createVersionOneDatabase(const std::string &path) {
  std::filesystem::remove(path);
  {
    facts::Storage current{path};
  }

  sqlite3 *database = nullptr;
  if (sqlite3_open(path.c_str(), &database) != SQLITE_OK) {
    return false;
  }
  const auto created = execute(database, R"sql(
DROP INDEX IF EXISTS idx_parameter_default_evaluated;
DROP TABLE parameter_default;
DROP INDEX IF EXISTS idx_variable_initializer_evaluated;
DROP TABLE variable_initializer;
ALTER TABLE symbol DROP COLUMN has_extern_storage;
PRAGMA user_version=1;
)sql");
  sqlite3_close(database);
  return created;
}

bool verifyVersionOneMigration(const std::string &path) {
  if (!require(createVersionOneDatabase(path),
               "failed to create version-one database")) {
    return false;
  }
  {
    facts::Storage migrated{path};
  }

  sqlite3 *database = nullptr;
  if (!require(sqlite3_open(path.c_str(), &database) == SQLITE_OK,
               "failed to open version-one database")) {
    return false;
  }
  const auto valid =
      require(scalar(database,
                     "SELECT COUNT(*) FROM pragma_table_info('symbol') "
                     "WHERE name='has_extern_storage'") == 1,
              "extern storage column was not migrated") &&
      require(scalar(database, "SELECT COUNT(*) FROM pragma_table_info("
                               "'variable_initializer')") == 4,
              "initializer table was not migrated from version one") &&
      require(scalar(database, "SELECT COUNT(*) FROM pragma_table_info("
                               "'parameter_default')") == 5,
              "parameter default table was not migrated from version one") &&
      require(scalar(database, "PRAGMA user_version") == 7,
              "version-one migration was not recorded");
  sqlite3_close(database);
  return valid;
}

bool createVersionTwoDatabase(const std::string &path) {
  std::filesystem::remove(path);
  {
    facts::Storage current{path};
  }

  sqlite3 *database = nullptr;
  if (sqlite3_open(path.c_str(), &database) != SQLITE_OK) {
    return false;
  }
  const auto created = execute(database, R"sql(
DROP INDEX IF EXISTS idx_parameter_default_evaluated;
DROP TABLE parameter_default;
PRAGMA user_version=2;
)sql");
  sqlite3_close(database);
  return created;
}

bool verifyVersionTwoMigration(const std::string &path) {
  if (!require(createVersionTwoDatabase(path),
               "failed to create version-two database")) {
    return false;
  }
  {
    facts::Storage migrated{path};
  }

  sqlite3 *database = nullptr;
  if (!require(sqlite3_open(path.c_str(), &database) == SQLITE_OK,
               "failed to open version-two database")) {
    return false;
  }
  const auto valid =
      require(scalar(database, "SELECT COUNT(*) FROM pragma_table_info("
                               "'parameter_default')") == 5,
              "parameter default table was not migrated from version two") &&
      require(scalar(database, "PRAGMA user_version") == 7,
              "version-two migration was not recorded");
  sqlite3_close(database);
  return valid;
}

bool verifyVersionFiveMigration(const std::string &path) {
  std::filesystem::remove(path);
  {
    facts::Storage current{path};
  }

  sqlite3 *database = nullptr;
  if (!require(sqlite3_open(path.c_str(), &database) == SQLITE_OK,
               "failed to open version-five database")) {
    return false;
  }
  const auto prepared = execute(database, R"sql(
DROP INDEX idx_symbol_unique_usr;
ALTER TABLE symbol ADD COLUMN identity TEXT NOT NULL DEFAULT '';
CREATE UNIQUE INDEX idx_symbol_file_identity
  ON symbol(((id >> 32) & 4294967295), identity);
CREATE UNIQUE INDEX idx_symbol_unique_usr ON symbol(usr) WHERE usr <> '';
PRAGMA user_version=5;
)sql");
  sqlite3_close(database);
  if (!require(prepared, "failed to create version-five database")) {
    return false;
  }

  {
    facts::Storage migrated{path};
  }
  if (!require(sqlite3_open(path.c_str(), &database) == SQLITE_OK,
               "failed to open migrated version-five database")) {
    return false;
  }
  const auto valid =
      require(usrIsOnlySymbolIdentity(database),
              "version-five migration retained a separate symbol identity") &&
      require(scalar(database, "PRAGMA user_version") == 7,
              "version-five migration was not recorded");
  sqlite3_close(database);
  return valid;
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
      require(noRedundantSymbolIdColumns(database),
              "migration retained redundant symbol id columns") &&
      require(usrIsOnlySymbolIdentity(database),
              "migration retained a separate symbol identity") &&
      require(scalar(database,
                     "SELECT COUNT(*) FROM symbol WHERE access='private' AND "
                     "is_definition=1 AND is_const=1 AND "
                     "ref_qualifier='rvalue' AND "
                     "has_extern_storage=1 AND "
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
      require(scalar(database, "SELECT COUNT(*) FROM pragma_table_info("
                               "'variable_initializer')") == 4,
              "initializer schema was not migrated") &&
      require(scalar(database, "SELECT COUNT(*) FROM pragma_table_info("
                               "'parameter_default')") == 5,
              "parameter default schema was not migrated") &&
      require(scalar(database, "SELECT COUNT(*) FROM pragma_table_info("
                               "'enumeration')") == 4,
              "enumeration schema was not migrated") &&
      require(scalar(database, "SELECT COUNT(*) FROM pragma_table_info("
                               "'enumerator')") == 3,
              "enumerator schema was not migrated") &&
      require(scalar(database, "SELECT COUNT(*) FROM pragma_table_info("
                               "'relation_site')") == 8,
              "relation-site schema was not migrated") &&
      require(scalar(database, "PRAGMA user_version") == 7,
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
  return verifyFreshSchema(argv[1]) && verifyMigration(argv[2]) &&
                 verifyVersionOneMigration(std::string{argv[2]} + ".v1") &&
                 verifyVersionTwoMigration(std::string{argv[2]} + ".v2") &&
                 verifyVersionFiveMigration(std::string{argv[2]} + ".v5")
             ? 0
             : 1;
}
