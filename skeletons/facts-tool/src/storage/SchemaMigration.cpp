#include "storage/SchemaMigration.h"

#include "storage/Sqlite.h"

#include <sqlite3.h>

#include <string_view>

namespace facts::storage {
namespace {

std::expected<bool, std::error_code>
hasColumn(sqlite3 *database, std::string_view table, std::string_view column) {
  auto statement =
      prepare(database, "PRAGMA table_info(" + std::string{table} + ")");
  if (!statement) {
    return std::unexpected(sqliteError(database));
  }

  auto step = sqlite3_step(statement->get());
  while (step == SQLITE_ROW) {
    if (columnText(statement->get(), 1) == column) {
      return true;
    }
    step = sqlite3_step(statement->get());
  }
  return step == SQLITE_DONE ? std::expected<bool, std::error_code>{false}
                             : std::unexpected(sqliteError(database));
}

std::expected<int, std::error_code> schemaVersion(sqlite3 *database) {
  auto statement = prepare(database, "PRAGMA user_version");
  if (!statement) {
    return std::unexpected(statement.error());
  }
  if (sqlite3_step(statement->get()) != SQLITE_ROW) {
    return std::unexpected(sqliteError(database));
  }
  return sqlite3_column_int(statement->get(), 0);
}

inline constexpr auto migrationSql = R"sql(
ALTER TABLE symbol ADD COLUMN access TEXT NOT NULL DEFAULT 'none'
  CHECK(access IN ('none','public','protected','private'));
ALTER TABLE symbol ADD COLUMN is_definition INTEGER NOT NULL DEFAULT 0 CHECK(is_definition IN (0,1));
ALTER TABLE symbol ADD COLUMN is_implicit INTEGER NOT NULL DEFAULT 0 CHECK(is_implicit IN (0,1));
ALTER TABLE symbol ADD COLUMN is_static INTEGER NOT NULL DEFAULT 0 CHECK(is_static IN (0,1));
ALTER TABLE symbol ADD COLUMN is_virtual INTEGER NOT NULL DEFAULT 0 CHECK(is_virtual IN (0,1));
ALTER TABLE symbol ADD COLUMN is_const INTEGER NOT NULL DEFAULT 0 CHECK(is_const IN (0,1));
ALTER TABLE symbol ADD COLUMN is_inline INTEGER NOT NULL DEFAULT 0 CHECK(is_inline IN (0,1));
ALTER TABLE symbol ADD COLUMN is_pure INTEGER NOT NULL DEFAULT 0 CHECK(is_pure IN (0,1));
ALTER TABLE symbol ADD COLUMN ref_qualifier TEXT NOT NULL DEFAULT 'none'
  CHECK(ref_qualifier IN ('none','lvalue','rvalue'));
ALTER TABLE symbol ADD COLUMN is_override INTEGER NOT NULL DEFAULT 0 CHECK(is_override IN (0,1));
ALTER TABLE symbol ADD COLUMN has_internal_linkage INTEGER NOT NULL DEFAULT 0 CHECK(has_internal_linkage IN (0,1));
ALTER TABLE symbol ADD COLUMN is_external INTEGER NOT NULL DEFAULT 0 CHECK(is_external IN (0,1));
ALTER TABLE symbol ADD COLUMN is_variadic INTEGER NOT NULL DEFAULT 0 CHECK(is_variadic IN (0,1));
ALTER TABLE symbol ADD COLUMN is_deleted INTEGER NOT NULL DEFAULT 0 CHECK(is_deleted IN (0,1));
ALTER TABLE symbol ADD COLUMN is_defaulted INTEGER NOT NULL DEFAULT 0 CHECK(is_defaulted IN (0,1));
ALTER TABLE symbol ADD COLUMN is_explicit INTEGER NOT NULL DEFAULT 0 CHECK(is_explicit IN (0,1));
ALTER TABLE symbol ADD COLUMN is_final INTEGER NOT NULL DEFAULT 0 CHECK(is_final IN (0,1));
ALTER TABLE symbol ADD COLUMN is_abstract INTEGER NOT NULL DEFAULT 0 CHECK(is_abstract IN (0,1));
ALTER TABLE symbol ADD COLUMN is_polymorphic INTEGER NOT NULL DEFAULT 0 CHECK(is_polymorphic IN (0,1));
ALTER TABLE symbol ADD COLUMN has_extern_storage INTEGER NOT NULL DEFAULT 0 CHECK(has_extern_storage IN (0,1));
ALTER TABLE symbol ADD COLUMN constant_evaluation TEXT NOT NULL DEFAULT 'none'
  CHECK(constant_evaluation IN ('none','constexpr','consteval','constinit'));
ALTER TABLE symbol ADD COLUMN is_noexcept INTEGER NOT NULL DEFAULT 0 CHECK(is_noexcept IN (0,1));
UPDATE symbol SET
  access=CASE (flags & 3) WHEN 0 THEN 'public' WHEN 1 THEN 'protected' WHEN 2 THEN 'private' ELSE 'none' END,
  is_definition=(flags >> 2) & 1,
  is_implicit=(flags >> 3) & 1,
  is_static=(flags >> 4) & 1,
  is_virtual=(flags >> 5) & 1,
  is_const=(flags >> 6) & 1,
  is_inline=(flags >> 7) & 1,
  is_pure=(flags >> 8) & 1,
  ref_qualifier=CASE ((flags >> 9) & 3) WHEN 1 THEN 'lvalue' WHEN 2 THEN 'rvalue' ELSE 'none' END,
  is_override=(flags >> 11) & 1,
  has_internal_linkage=(flags >> 12) & 1,
  is_external=(flags >> 13) & 1,
  is_variadic=(flags >> 14) & 1,
  is_deleted=(flags >> 15) & 1,
  is_defaulted=(flags >> 16) & 1,
  is_explicit=(flags >> 17) & 1,
  is_final=(flags >> 18) & 1,
  is_abstract=(flags >> 19) & 1,
  is_polymorphic=(flags >> 20) & 1,
  has_extern_storage=(flags >> 21) & 1,
  constant_evaluation=CASE ((flags >> 22) & 3) WHEN 1 THEN 'constexpr' WHEN 2 THEN 'consteval' WHEN 3 THEN 'constinit' ELSE 'none' END,
  is_noexcept=(flags >> 24) & 1;
ALTER TABLE symbol DROP COLUMN flags;

ALTER TABLE parameter ADD COLUMN is_pointer INTEGER NOT NULL DEFAULT 0 CHECK(is_pointer IN (0,1));
ALTER TABLE parameter ADD COLUMN is_lvalue_reference INTEGER NOT NULL DEFAULT 0 CHECK(is_lvalue_reference IN (0,1));
ALTER TABLE parameter ADD COLUMN is_rvalue_reference INTEGER NOT NULL DEFAULT 0 CHECK(is_rvalue_reference IN (0,1));
ALTER TABLE parameter ADD COLUMN is_forwarding_reference INTEGER NOT NULL DEFAULT 0 CHECK(is_forwarding_reference IN (0,1));
ALTER TABLE parameter ADD COLUMN is_const INTEGER NOT NULL DEFAULT 0 CHECK(is_const IN (0,1));
ALTER TABLE parameter ADD COLUMN is_pack INTEGER NOT NULL DEFAULT 0 CHECK(is_pack IN (0,1));
UPDATE parameter SET
  is_pointer=flags & 1,
  is_lvalue_reference=(flags >> 1) & 1,
  is_rvalue_reference=(flags >> 2) & 1,
  is_forwarding_reference=(flags >> 3) & 1,
  is_const=(flags >> 4) & 1,
  is_pack=(flags >> 5) & 1;
ALTER TABLE parameter DROP COLUMN flags;

ALTER TABLE relation ADD COLUMN access TEXT NOT NULL DEFAULT 'none'
  CHECK(access IN ('none','public','protected','private'));
ALTER TABLE relation ADD COLUMN is_virtual_base INTEGER NOT NULL DEFAULT 0 CHECK(is_virtual_base IN (0,1));
ALTER TABLE relation ADD COLUMN is_implicit INTEGER NOT NULL DEFAULT 0 CHECK(is_implicit IN (0,1));
ALTER TABLE relation ADD COLUMN is_lexical INTEGER NOT NULL DEFAULT 0 CHECK(is_lexical IN (0,1));
UPDATE relation SET
  access=CASE (flags & 3) WHEN 0 THEN 'public' WHEN 1 THEN 'protected' WHEN 2 THEN 'private' ELSE 'none' END,
  is_virtual_base=(flags >> 2) & 1,
  is_implicit=(flags >> 3) & 1,
  is_lexical=(flags >> 4) & 1;
ALTER TABLE relation DROP COLUMN flags;

ALTER TABLE template_argument ADD COLUMN is_parameter_pack INTEGER NOT NULL DEFAULT 0 CHECK(is_parameter_pack IN (0,1));
ALTER TABLE template_argument ADD COLUMN is_non_type INTEGER NOT NULL DEFAULT 0 CHECK(is_non_type IN (0,1));
ALTER TABLE template_argument ADD COLUMN is_template_template INTEGER NOT NULL DEFAULT 0 CHECK(is_template_template IN (0,1));
UPDATE template_argument SET
  is_parameter_pack=flags & 1,
  is_non_type=(flags >> 1) & 1,
  is_template_template=(flags >> 2) & 1;
ALTER TABLE template_argument DROP COLUMN flags;

ALTER TABLE template_parameter ADD COLUMN is_pointer INTEGER NOT NULL DEFAULT 0 CHECK(is_pointer IN (0,1));
ALTER TABLE template_parameter ADD COLUMN is_lvalue_reference INTEGER NOT NULL DEFAULT 0 CHECK(is_lvalue_reference IN (0,1));
ALTER TABLE template_parameter ADD COLUMN is_rvalue_reference INTEGER NOT NULL DEFAULT 0 CHECK(is_rvalue_reference IN (0,1));
ALTER TABLE template_parameter ADD COLUMN is_forwarding_reference INTEGER NOT NULL DEFAULT 0 CHECK(is_forwarding_reference IN (0,1));
ALTER TABLE template_parameter ADD COLUMN is_const INTEGER NOT NULL DEFAULT 0 CHECK(is_const IN (0,1));
ALTER TABLE template_parameter ADD COLUMN is_pack INTEGER NOT NULL DEFAULT 0 CHECK(is_pack IN (0,1));
UPDATE template_parameter SET
  is_pointer=flags & 1,
  is_lvalue_reference=(flags >> 1) & 1,
  is_rvalue_reference=(flags >> 2) & 1,
  is_forwarding_reference=(flags >> 3) & 1,
  is_const=(flags >> 4) & 1,
  is_pack=(flags >> 5) & 1;
ALTER TABLE template_parameter DROP COLUMN flags;

CREATE TABLE IF NOT EXISTS variable_initializer (
  symbol_id       INTEGER PRIMARY KEY REFERENCES symbol(id) ON DELETE CASCADE,
  expression      TEXT NOT NULL,
  evaluated_kind  TEXT NOT NULL
    CHECK(evaluated_kind IN ('none','integer','floating','boolean','string')),
  evaluated_value TEXT,
  CHECK((evaluated_kind='none' AND evaluated_value IS NULL) OR
        (evaluated_kind<>'none' AND evaluated_value IS NOT NULL))
);
CREATE INDEX IF NOT EXISTS idx_variable_initializer_evaluated
  ON variable_initializer(evaluated_kind,evaluated_value)
  WHERE evaluated_kind<>'none';

PRAGMA user_version=2;
)sql";

inline constexpr auto initializerMigrationSql = R"sql(
ALTER TABLE symbol ADD COLUMN has_extern_storage INTEGER NOT NULL DEFAULT 0
  CHECK(has_extern_storage IN (0,1));
CREATE TABLE IF NOT EXISTS variable_initializer (
  symbol_id       INTEGER PRIMARY KEY REFERENCES symbol(id) ON DELETE CASCADE,
  expression      TEXT NOT NULL,
  evaluated_kind  TEXT NOT NULL
    CHECK(evaluated_kind IN ('none','integer','floating','boolean','string')),
  evaluated_value TEXT,
  CHECK((evaluated_kind='none' AND evaluated_value IS NULL) OR
        (evaluated_kind<>'none' AND evaluated_value IS NOT NULL))
);
CREATE INDEX IF NOT EXISTS idx_variable_initializer_evaluated
  ON variable_initializer(evaluated_kind,evaluated_value)
  WHERE evaluated_kind<>'none';
PRAGMA user_version=2;
)sql";

inline constexpr auto parameterDefaultMigrationSql = R"sql(
CREATE TABLE IF NOT EXISTS parameter_default (
  symbol_id       INTEGER NOT NULL,
  position        INTEGER NOT NULL,
  expression      TEXT NOT NULL,
  evaluated_kind  TEXT NOT NULL
    CHECK(evaluated_kind IN ('none','integer','floating','boolean','string')),
  evaluated_value TEXT,
  PRIMARY KEY (symbol_id, position),
  FOREIGN KEY (symbol_id, position) REFERENCES parameter(symbol_id, position)
    ON DELETE CASCADE,
  CHECK((evaluated_kind='none' AND evaluated_value IS NULL) OR
        (evaluated_kind<>'none' AND evaluated_value IS NOT NULL))
) WITHOUT ROWID;
CREATE INDEX IF NOT EXISTS idx_parameter_default_evaluated
  ON parameter_default(evaluated_kind,evaluated_value)
  WHERE evaluated_kind<>'none';
PRAGMA user_version=3;
)sql";

inline constexpr auto enumerationMigrationSql = R"sql(
CREATE TABLE IF NOT EXISTS enumeration (
  symbol_id INTEGER PRIMARY KEY REFERENCES symbol(id) ON DELETE CASCADE,
  underlying_type INTEGER NOT NULL,
  is_scoped INTEGER NOT NULL CHECK(is_scoped IN (0,1)),
  has_fixed_underlying_type INTEGER NOT NULL
    CHECK(has_fixed_underlying_type IN (0,1))
);
CREATE TABLE IF NOT EXISTS enumerator (
  symbol_id INTEGER PRIMARY KEY REFERENCES symbol(id) ON DELETE CASCADE,
  value TEXT NOT NULL,
  initializer_expression TEXT NOT NULL
);
PRAGMA user_version=4;
)sql";

inline constexpr auto packedSymbolIdMigrationSql = R"sql(
CREATE TABLE symbol_compact (
  id             INTEGER PRIMARY KEY,
  node           INTEGER NOT NULL,
  kind           INTEGER NOT NULL,
  sub_kind       INTEGER NOT NULL,
  lang           INTEGER NOT NULL,
  properties     INTEGER NOT NULL,
  usr            TEXT NOT NULL CHECK(usr <> ''),
  qualified_name TEXT NOT NULL,
  line           INTEGER NOT NULL,
  col            INTEGER NOT NULL,
  offset         INTEGER NOT NULL,
  access         TEXT NOT NULL
    CHECK(access IN ('none','public','protected','private')),
  is_definition  INTEGER NOT NULL CHECK(is_definition IN (0,1)),
  is_implicit    INTEGER NOT NULL CHECK(is_implicit IN (0,1)),
  is_static      INTEGER NOT NULL CHECK(is_static IN (0,1)),
  is_virtual     INTEGER NOT NULL CHECK(is_virtual IN (0,1)),
  is_const       INTEGER NOT NULL CHECK(is_const IN (0,1)),
  is_inline      INTEGER NOT NULL CHECK(is_inline IN (0,1)),
  is_pure        INTEGER NOT NULL CHECK(is_pure IN (0,1)),
  ref_qualifier  TEXT NOT NULL
    CHECK(ref_qualifier IN ('none','lvalue','rvalue')),
  is_override    INTEGER NOT NULL CHECK(is_override IN (0,1)),
  has_internal_linkage INTEGER NOT NULL
    CHECK(has_internal_linkage IN (0,1)),
  is_external    INTEGER NOT NULL CHECK(is_external IN (0,1)),
  is_variadic    INTEGER NOT NULL CHECK(is_variadic IN (0,1)),
  is_deleted     INTEGER NOT NULL CHECK(is_deleted IN (0,1)),
  is_defaulted   INTEGER NOT NULL CHECK(is_defaulted IN (0,1)),
  is_explicit    INTEGER NOT NULL CHECK(is_explicit IN (0,1)),
  is_final       INTEGER NOT NULL CHECK(is_final IN (0,1)),
  is_abstract    INTEGER NOT NULL CHECK(is_abstract IN (0,1)),
  is_polymorphic INTEGER NOT NULL CHECK(is_polymorphic IN (0,1)),
  has_extern_storage INTEGER NOT NULL CHECK(has_extern_storage IN (0,1)),
  constant_evaluation TEXT NOT NULL
    CHECK(constant_evaluation IN ('none','constexpr','consteval','constinit')),
  is_noexcept    INTEGER NOT NULL CHECK(is_noexcept IN (0,1))
);

INSERT INTO symbol_compact(
  id,node,kind,sub_kind,lang,properties,usr,qualified_name,line,col,
  offset,access,is_definition,is_implicit,is_static,is_virtual,is_const,
  is_inline,is_pure,ref_qualifier,is_override,has_internal_linkage,is_external,
  is_variadic,is_deleted,is_defaulted,is_explicit,is_final,is_abstract,
  is_polymorphic,has_extern_storage,constant_evaluation,is_noexcept)
SELECT
  id,node,kind,sub_kind,lang,properties,usr,qualified_name,line,col,
  offset,access,is_definition,is_implicit,is_static,is_virtual,is_const,
  is_inline,is_pure,ref_qualifier,is_override,has_internal_linkage,is_external,
  is_variadic,is_deleted,is_defaulted,is_explicit,is_final,is_abstract,
  is_polymorphic,has_extern_storage,constant_evaluation,is_noexcept
FROM symbol;

DROP TABLE symbol;
ALTER TABLE symbol_compact RENAME TO symbol;
PRAGMA user_version=6;
)sql";

inline constexpr auto usrIdentityMigrationSql = R"sql(
DROP INDEX IF EXISTS idx_symbol_file_identity;
DROP INDEX IF EXISTS idx_symbol_unique_usr;
ALTER TABLE symbol DROP COLUMN identity;
CREATE UNIQUE INDEX idx_symbol_unique_usr ON symbol(usr);
PRAGMA user_version=6;
)sql";

inline constexpr auto relationSiteMigrationSql = R"sql(
CREATE TABLE IF NOT EXISTS relation_site (
  source_id      INTEGER NOT NULL,
  destination_id INTEGER NOT NULL,
  kind           INTEGER NOT NULL,
  position       INTEGER NOT NULL DEFAULT 0,
  file_id        INTEGER NOT NULL,
  line           INTEGER NOT NULL,
  col            INTEGER NOT NULL,
  offset         INTEGER NOT NULL,
  PRIMARY KEY (source_id, destination_id, kind, position, file_id, offset),
  FOREIGN KEY (source_id, destination_id, kind, position)
    REFERENCES relation(source_id, destination_id, kind, position)
    ON DELETE CASCADE
) WITHOUT ROWID;
PRAGMA user_version=7;
)sql";

inline constexpr auto relationSiteContextMigrationSql = R"sql(
ALTER TABLE relation_site
ADD COLUMN receiver_type_id INTEGER REFERENCES symbol(id);
ALTER TABLE relation_site
ADD COLUMN certainty INTEGER;
PRAGMA user_version=8;
)sql";

} // namespace

std::expected<void, std::error_code> migrateSchema(sqlite3 *database) {
  return hasColumn(database, "symbol", "id")
      .and_then([database](bool existing) {
        if (!existing) {
          return std::expected<void, std::error_code>{};
        }
        return hasColumn(database, "symbol", "flags")
            .and_then([database](bool legacy) {
              return legacy ? execute(database, migrationSql)
                            : std::expected<void, std::error_code>{};
            })
            .and_then([database] {
              return schemaVersion(database).and_then([database](int version) {
                return version < 2 ? execute(database, initializerMigrationSql)
                                   : std::expected<void, std::error_code>{};
              });
            })
            .and_then([database] {
              return schemaVersion(database).and_then([database](int version) {
                return version < 3
                           ? execute(database, parameterDefaultMigrationSql)
                           : std::expected<void, std::error_code>{};
              });
            })
            .and_then([database] {
              return schemaVersion(database).and_then([database](int version) {
                return version < 4 ? execute(database, enumerationMigrationSql)
                                   : std::expected<void, std::error_code>{};
              });
            })
            .and_then([database] {
              return schemaVersion(database).and_then([database](int version) {
                return version < 5
                           ? execute(database, packedSymbolIdMigrationSql)
                           : std::expected<void, std::error_code>{};
              });
            })
            .and_then([database] {
              return schemaVersion(database).and_then([database](int version) {
                return version < 6 ? execute(database, usrIdentityMigrationSql)
                                   : std::expected<void, std::error_code>{};
              });
            })
            .and_then([database] {
              return schemaVersion(database).and_then([database](int version) {
                return version < 7 ? execute(database, relationSiteMigrationSql)
                                   : std::expected<void, std::error_code>{};
              });
            })
            .and_then([database] {
              return schemaVersion(database).and_then([database](int version) {
                return version < 8
                           ? execute(database, relationSiteContextMigrationSql)
                           : std::expected<void, std::error_code>{};
              });
            });
      });
}

} // namespace facts::storage
