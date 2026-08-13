// storage/Schema.h — the SQLite tables the fact model lands in.
//
// One table per shape in model/, and the mapping is deliberately dull: a
// PO field is a column, an inline vector is a side table keyed by (owner,
// position), and the variant alternative is one integer on the symbol row.
//
// SymbolId is two 32-bit halves, and SQLite keys are one 64-bit integer, so
// every id column is the pair packed: (int64(file) << 32) | index. That keeps
// `id INTEGER PRIMARY KEY` — the rowid alias, the only key SQLite stores
// without a second B-tree — and still lets `id >> 32` ask which file a symbol
// came from without a join. Zero is the null id: the type of `int`, the file of
// a symbol that has none.
//
// FileId allocation lives in FileManager's separate SQLite registry. This
// database only persists facts and retains the allocated FileId values.
//
// No schema-version table, no timestamps, no counts a COUNT(*) answers.

#ifndef FACTS_TOOL_STORAGE_SCHEMA_H
#define FACTS_TOOL_STORAGE_SCHEMA_H

namespace facts {

inline constexpr const char *schemaSql = R"sql(

-- The next per-file SymbolId index. Allocation happens under BEGIN IMMEDIATE,
-- so independent extractor processes cannot mint the same index for different
-- declarations from a shared header.
CREATE TABLE IF NOT EXISTS symbol_allocator (
  file_id    INTEGER PRIMARY KEY,
  next_index INTEGER NOT NULL
) WITHOUT ROWID;

-- The common row: everything in Symbol, which every alternative starts with.
-- kind/sub_kind/lang/properties are the clang::index::SymbolInfo base, stored
-- as the raw enum values index::getSymbolInfo() produced.
CREATE TABLE IF NOT EXISTS symbol (
  id             INTEGER PRIMARY KEY,
  file_id        INTEGER NOT NULL,
  file_index     INTEGER NOT NULL,
  identity       TEXT NOT NULL,
  node           INTEGER NOT NULL,  -- which Storage specialization owns it,
                                    -- and therefore which side tables apply
  kind           INTEGER NOT NULL,
  sub_kind       INTEGER NOT NULL,
  lang           INTEGER NOT NULL,
  properties     INTEGER NOT NULL,
  usr            TEXT NOT NULL,
  qualified_name TEXT NOT NULL,
  line           INTEGER NOT NULL,  -- Symbol::loc — where it was declared;
  col            INTEGER NOT NULL,  -- the file is already the top half of id
  offset         INTEGER NOT NULL,
  flags          INTEGER NOT NULL,  -- access in bits 0-1, SymbolBit above it
  UNIQUE(file_id, file_index),
  UNIQUE(file_id, identity)
);

-- '%name%' over qualified_name is the search, so it is the one index that
-- earns its keep beyond the keys; usr is how a merge or a second run finds a
-- symbol it already knows.
CREATE INDEX IF NOT EXISTS idx_symbol_qualified_name ON symbol(qualified_name);
CREATE UNIQUE INDEX IF NOT EXISTS idx_symbol_unique_usr ON symbol(usr)
  WHERE usr <> '';

-- Where the body is. A side table and not columns on symbol because only
-- records and functions can have one, and because a missing row already says
-- "declared, never defined" — no size-0 sentinel needed. file_id is separate:
-- a method declared in a header is defined in a .cpp.
CREATE TABLE IF NOT EXISTS definition (
  symbol_id INTEGER PRIMARY KEY REFERENCES symbol(id) ON DELETE CASCADE,
  file_id   INTEGER NOT NULL,
  offset    INTEGER NOT NULL,
  size      INTEGER NOT NULL
);

-- Symbol::parameters. Type is a packed SymbolId; FileId zero identifies
-- predefined compiler types.
CREATE TABLE IF NOT EXISTS parameter (
  symbol_id    INTEGER NOT NULL REFERENCES symbol(id) ON DELETE CASCADE,
  position     INTEGER NOT NULL,
  name         TEXT NOT NULL,
  type         INTEGER NOT NULL,
  line         INTEGER NOT NULL,
  col          INTEGER NOT NULL,
  offset       INTEGER NOT NULL,
  region_offset INTEGER NOT NULL,
  region_size   INTEGER NOT NULL,
  flags         INTEGER NOT NULL,
  has_default   INTEGER NOT NULL,
  PRIMARY KEY (symbol_id, position)
) WITHOUT ROWID;

-- The slots a template declares: `template <typename T, int N>`. Named the way
-- the fact model names it, which is the opposite of the standard's wording and
-- of cpp-indexer's template_param/template_arg tables -- here an argument is
-- the declared slot and a parameter is the value supplied for it.
CREATE TABLE IF NOT EXISTS template_argument (
  symbol_id INTEGER NOT NULL REFERENCES symbol(id) ON DELETE CASCADE,
  position  INTEGER NOT NULL,
  name      TEXT NOT NULL,     -- 'T', 'U', 'N'
  type_id   INTEGER NOT NULL,  -- a non-type slot's own type; 0 for builtins
  flags     INTEGER NOT NULL,  -- TemplateArgumentBit
  PRIMARY KEY (symbol_id, position)
) WITHOUT ROWID;

-- The values supplied to those slots: the `int` and `std::string` of
-- `Value<int, std::string>`.
CREATE TABLE IF NOT EXISTS template_parameter (
  symbol_id  INTEGER NOT NULL REFERENCES symbol(id) ON DELETE CASCADE,
  position   INTEGER NOT NULL,
  value      TEXT NOT NULL,     -- '4', 'Mode::Write'; '' for a type value
  type_id    INTEGER NOT NULL,
  flags      INTEGER NOT NULL,  -- the same ParameterMode / ParameterBit layout
  kind       INTEGER NOT NULL,  -- TemplateParameterKind
  pack_index INTEGER NOT NULL,
  PRIMARY KEY (symbol_id, position)
) WITHOUT ROWID;

-- TypeAlias::underlyingType. One column, so it could have been a nullable
-- field on symbol, but an alias is a handful of rows in a table of millions.
CREATE TABLE IF NOT EXISTS type_alias (
  symbol_id     INTEGER PRIMARY KEY REFERENCES symbol(id) ON DELETE CASCADE,
  underlying_id INTEGER NOT NULL
) WITHOUT ROWID;

-- Every edge. position is in the key because an ordered kind can join the same
-- pair twice -- a function taking Widget at position 1 and again at 3 is two
-- ParamType edges, not one with count 2.
CREATE TABLE IF NOT EXISTS relation (
  source_id      INTEGER NOT NULL REFERENCES symbol(id) ON DELETE CASCADE,
  destination_id INTEGER NOT NULL REFERENCES symbol(id) ON DELETE CASCADE,
  kind           INTEGER NOT NULL,  -- RelationKind
  position       INTEGER NOT NULL,  -- 0 when the kind is unordered
  flags          INTEGER NOT NULL,  -- base access in bits 0-1, RelationBit above
  count          INTEGER NOT NULL,
  PRIMARY KEY (source_id, destination_id, kind, position)
) WITHOUT ROWID;

-- Forwards is the primary key's own prefix; backwards -- who calls this, who
-- derives from this -- needs its own.
CREATE INDEX IF NOT EXISTS idx_relation_destination
  ON relation(destination_id, kind);

)sql";

} // namespace facts

#endif // FACTS_TOOL_STORAGE_SCHEMA_H
