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
- The template tables use the exact flag vocabularies from
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

On open, `storage/SchemaMigration.cpp` detects legacy packed flags and applies
versioned upgrades inside the storage connection's `BEGIN IMMEDIATE`
transaction. Existing identities and facts are preserved. The complete fresh
schema is defined by `storage/Schema.h`; fresh databases are created directly
at SQLite `user_version = 12` without packed persisted flags.

The version-8-to-9 migration adds `callable_return_type`, keyed by `symbol_id`
with a cascading foreign key to `symbol`. Its nonempty `canonical_type` text
preserves the full return spelling, including pointer, reference and const
qualification. A `relation` edge with `kind = 21` (`ReturnType`) identifies the
resolved target type separately. Functions, methods, lambda closure call
operators and function-object call operators share this representation;
constructors/destructors have no return type, and undeduced template returns
wait for a concrete specialization.

Predefined primitive targets use the same fixed FileId-0 IDs as parameter types.
Their implicit, external `symbol` rows satisfy relation foreign keys, but are
excluded from declaration listings and the symbol browser. Explicit named
queries can still inspect them. Return-type edges and spelling are replaced
together, and extraction rollback or deleting a callable also rolls back or
removes its return facts.

Compiler-provided implicit callable targets with no usable declaration location
also use FileId zero. Dynamic indices start above every reserved
`clang::BuiltinType::Kind + 1` index, including primitives without rows, and
above every occupied FileId-0 ID. Allocation preserves a higher recorded
`symbol_allocator.next_index`, checks 32-bit exhaustion, and shares the ordinary
transaction and unique-USR save path. It needs no schema migration; reopening
an existing database reserves the range atomically on its next dynamic save.
These symbols retain the actual USR, semantic function kind, implicit/external
flags and callable properties, with zero declaration coordinates and no
definition row or physical file. Caller sites keep their real file and location.
Usable redeclarations and ordinary external targets retain file-backed behavior.
See [compiler-provided callables](compiler-provided-callables.md) for scope,
traversal boundaries and regression evidence.

Version 9-to-10 adds `symbol.is_volatile` without changing identities; older
supported layouts still pass through the existing migration chain.

Migration does not invent return spellings for previously indexed functions;
re-extract the source to populate those facts. Symbol queries can read older
databases without migrating or writing them, using existing target names when
available. `FileSchemaMigration` belongs to the separate project configuration
registry and is not involved in facts-schema upgrades. That registry has its
own `project_registry.schema_version`; version 1 adds the match-only four-column
discovery index without changing facts `user_version`. See the
[matched-symbol index](matched-symbol-index.md).

Readback composes the explicit columns into the original compact in-memory
flags, preserving the public C++ model and extraction behavior.

## Version 11-to-12: persisted call-graph runs

Version 12 adds the append-only call-graph run history tables: `callgraph_run`,
`callgraph_run_root`, `callgraph_run_target`, `callgraph_run_edge`,
`callgraph_run_frontier`, and `callgraph_run_recovery` (columns documented in
[call-graph.md](call-graph.md)). Fresh schema creation includes them
directly; the migration adds them to an existing schema-11 database the next
time any command opens that facts store read/write, so a store extracted
before this change gains the tables at its next `extract` or `analyse
call-graph` invocation even before that invocation records any symbols or
runs a traversal. Rows are appended only by a subsequent `analyse call-graph`
invocation that reaches traversal; no migration and no other command deletes
or rewrites an existing run.
