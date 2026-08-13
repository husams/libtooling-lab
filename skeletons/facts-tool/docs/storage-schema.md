# Queryable semantic storage

The facts-tool keeps compact bit fields in `Symbol`, `Parameter`, and
`Relation` while the SQLite schema stores each semantic property in a named,
constrained column. This preserves the small extraction model without making
database clients decode implementation-specific bit positions.

## Stored properties

- `symbol` stores access as `none`, `public`, `protected`, or `private`; its
  single-bit properties as checked integer booleans; reference qualification
  as `none`, `lvalue`, or `rvalue`; and constant evaluation as `none`,
  `constexpr`, `consteval`, or `constinit`.
- `parameter` stores pointer, lvalue-reference, rvalue-reference, forwarding
  reference, const, and pack properties as checked integer booleans. Separate
  columns retain the model's legal overlap between rvalue and forwarding
  references.
- `relation` stores access plus virtual-base, implicit-edge, and lexical-edge
  booleans.
- The currently inactive template tables use the exact flag vocabularies from
  their repository-defined models: template arguments expose parameter-pack,
  non-type, and template-template booleans; template parameters expose the
  same six properties as function parameters.

All boolean columns are constrained to `0` or `1`, and all textual enum
columns have `CHECK` constraints listing their accepted values.

## Migration decision

Persisted facts databases are expected to survive tool invocations. The
storage connection opens an existing path read/write, symbol writes upsert
existing identities, and the executable stability scenarios run extraction
again against the same database. Requiring deletion would break that existing
workflow.

On open, schema migration detects the legacy `symbol.flags` column. A single
`BEGIN IMMEDIATE` transaction then adds and backfills every explicit property
column, drops all five opaque `flags` columns, records SQLite
`user_version = 1`, and commits. The template-table layouts are migrated too,
even though they have no active writer in the current skeleton, so a populated
legacy database does not lose those properties. Fresh databases are created
directly at version 1 and never contain a packed persisted flag.

Readback composes the explicit columns into the original compact in-memory
flags, preserving the public C++ model and extraction behavior.
