# Storage schema

`facts-tool` always writes into two SQLite databases: the **facts database**
and the **project (configuration) database**. This chapter documents every
table and column in both and their current schema versions for conceptual
reference; it is not a database-access interface.

**Never query or modify either database directly.** Agents must use the
public Python SDK for programmatic reads and native facts-tool commands for
matching, extraction, and catalog mutations. SQL, `sqlite3`, other database
drivers, and private SDK connections are prohibited, including diagnostic
reads. If the public SDK cannot expose a required fact, report the capability
gap. The native writer owns pairing, invalidation, and history invariants.

Source of truth for both schemas: `src/storage/Schema.h` (facts database),
`src/storage/FileSchema.h` (project database's base tables), and
`src/storage/ProjectSchemaMigration.cpp` (project database's `schema_version`
1 migration, `matched_symbol_index`).

## Facts database

Fresh facts databases are created directly at SQLite `user_version = 12`.
Migration on open (`storage/SchemaMigration.cpp`, inside `BEGIN IMMEDIATE`)
preserves existing rows and identities; it never invents historical data -
for example a migrated `is_volatile` reads `0` meaning *unknown*, not
*confirmed false*.

### `symbol`

The common row every declaration shares.

| Column | Type | Meaning |
|---|---|---|
| `id` | `INTEGER PRIMARY KEY` | Packed `SymbolId`: `(int64(file_id) << 32) \| index`. `id >> 32` recovers the owning `FileId` with no join. |
| `node` | `INTEGER` | Which model `Storage` specialization owns this row (which side tables apply) |
| `kind`, `sub_kind`, `lang`, `properties` | `INTEGER` | Raw `clang::index::SymbolInfo` fields as Clang's indexer produced them |
| `usr` | `TEXT`, non-empty | Clang USR - the sole symbol identity; unique index `idx_symbol_unique_usr` |
| `qualified_name` | `TEXT` | Human-facing name; indexed (`idx_symbol_qualified_name`) for substring search |
| `line`, `col`, `offset` | `INTEGER` | Declaration location (file is the high half of `id`) |
| `access` | `TEXT` `CHECK IN ('none','public','protected','private')` | |
| `is_definition`, `is_implicit`, `is_static`, `is_virtual`, `is_const`, `is_inline`, `is_pure`, `is_override`, `has_internal_linkage`, `is_external`, `is_variadic`, `is_deleted`, `is_defaulted`, `is_explicit`, `is_final`, `is_abstract`, `is_polymorphic`, `has_extern_storage`, `is_noexcept` | `INTEGER` `CHECK IN (0,1)` | Checked booleans |
| `ref_qualifier` | `TEXT` `CHECK IN ('none','lvalue','rvalue')` | Method ref-qualifier |
| `constant_evaluation` | `TEXT` `CHECK IN ('none','constexpr','consteval','constinit')` | Enum, not bit flags |
| `is_volatile` | `INTEGER` `CHECK IN (0,1)`, default `0` | Added at schema 10; `0` on migrated rows means *never recorded*, not *confirmed non-volatile* |

### Other facts tables

| Table | Key columns | Meaning |
|---|---|---|
| `symbol_allocator` | `file_id PK`, `next_index` | Next per-file `SymbolId` index; allocated under `BEGIN IMMEDIATE` |
| `include_dependency` | `(src_file_id, dst_file_id) PK` | Direct include facts from `analyse dependency` |
| `callable_return_type` | `symbol_id PK` -> `symbol(id)` cascade, `canonical_type` | Full return-type spelling (schema 9+); a `relation` edge `kind=21` (`ReturnType`) identifies the resolved target type |
| `definition` | `symbol_id PK` -> `symbol(id)` cascade, `file_id`, `offset`, `size` | Where the body is; only records/functions get a row. A missing row means "declared, never defined." |
| `enumeration` | `symbol_id PK`, `underlying_type`, `is_scoped`, `has_fixed_underlying_type` | Enum-only facts; `underlying_type` is a packed `SymbolId` |
| `enumerator` | `symbol_id PK`, `value`, `initializer_expression` | |
| `variable_initializer` | `symbol_id PK`, `expression`, `evaluated_kind` `CHECK IN ('none','integer','floating','boolean','string')`, `evaluated_value` | `evaluated_value` is `NULL` iff `evaluated_kind='none'` |
| `parameter` | `(symbol_id, position) PK`, `name`, `type`, `line`, `col`, `offset`, `region_offset`, `region_size`, `is_pointer`, `is_lvalue_reference`, `is_rvalue_reference`, `is_forwarding_reference`, `is_const`, `is_pack`, `has_default` | `type` is a packed `SymbolId`; separate reference-kind columns preserve the model's legal rvalue/forwarding-reference overlap |
| `parameter_default` | `(symbol_id, position) PK` -> `parameter` cascade, `expression`, `evaluated_kind`, `evaluated_value` | Same evaluated-value contract as `variable_initializer` |
| `template_argument` | `(symbol_id, position) PK`, `name`, `type_id`, `is_parameter_pack`, `is_non_type`, `is_template_template` | The **declared slots** (`T`, `Args...`) - despite the name, this is the "parameter" side in ordinary C++ vocabulary. See the reversed-vocabulary note below. |
| `template_parameter` | `(symbol_id, position) PK`, `value`, `type_id`, reference/const/pack flags, `kind`, `pack_index` | The **supplied values** (`int`, `7`) - the "argument" side. |
| `relation` | `(source_id, destination_id, kind, position) PK` | Every edge; `access`, `is_virtual_base`, `is_implicit`, `is_lexical`, `count`. `position` is part of the key so an ordered kind can join the same pair twice (e.g. `Widget` at parameter positions 1 and 3). |
| `relation_site` | `(source_id, destination_id, kind, position, file_id, offset) PK` -> `relation` cascade | One occurrence's exact `line`/`col`/`offset`, plus `receiver_type_id` and `certainty` (nullable, added at schema 8) |
| `callgraph_entry` | `symbol_id PK` -> `symbol(id)` cascade, `graph_node_ref` (`CHECK symbol_id = graph_node_ref`) | One row per function whose call evidence has been fully committed at least once (schema 11+) |
| `callgraph_external_reference` | `(source_id, destination_id, kind, position, file_id, offset) PK` -> `relation_site` cascade, `external_symbol_id` (`CHECK = destination_id`) | A relation site pointing at a retained external symbol until a compatible project definition is found (schema 11+) |
| `callgraph_unresolved_site` | `(source_id, file_id, offset) PK` -> `symbol(id)` cascade, `line`, `col` | An indirect/unresolved call site with **no** destination field - "cannot invent an external target" (schema 11+) |
| `facts_project_provenance` | `file_id PK`, `path`, `universe_key` | Canonical registered path + semantic-universe key per FileId (schema 11+); the native writer's stronger identity evidence for rejecting incompatible pairs |

**`relation` kind values** (23 total; full list with Python names in
`python/docs/relations.md`): `calls=1`, `inherits=2`, `contains=3`,
`specializes=4`, `instantiates=5`, `overrides=6`, `uses=7`, `field_of=8`,
`method_of=9`, `construct_value/temp/heap/copy/move=10-14`,
`factory_construct=15`, `destroy=16`, `friend=17`, `dispatch_calls=18`,
`alias_of=19`, `of_type=20`, `return_type=21`, `param_type=22`,
`template_argument_type=23`. `Calls` (1) is a statically-selected callee;
`DispatchCalls` (18) is a conservative virtual-dispatch target. A proven
concrete by-value receiver gets `certainty=exact` on its site; pointer,
reference, implicit, or otherwise unproven receivers get
`certainty=possible`. There is no persisted `unknown` certainty: the stored
column is an integer, `1` for exact and `2` for possible, and it is `NULL`
when no receiver was recorded.

### Call-graph run tables (schema 12)

One row-set per `analyse call-graph` invocation that reaches traversal.
**Append-only** - a run is never rewritten or deleted by a later invocation.

| Table | Columns | Notes |
|---|---|---|
| `callgraph_run` | `run_id PK`, `created_at`, `project_path`, `facts_path`, `mode CHECK IN ('callees','callers','path')`, `path_mode CHECK IN ('shortest','all-simple') NULL`, `calls_scope CHECK IN ('all','project','library')`, `components` (comma-joined selected names, `''` for all), `max_depth`, `max_nodes`, `max_edges`, `time_limit_ms`, `recover_missing CHECK IN (0,1)`, `status CHECK IN ('complete','truncated','cancelled','recovery-failed','failed')`, `truncation_reason`, `error` | One row per run |
| `callgraph_run_root` | `(run_id, symbol_id) PK` -> `callgraph_run` cascade, `usr` | Selected roots (every definition-backed root for `--all`) |
| `callgraph_run_target` | `(run_id, symbol_id) PK` -> `callgraph_run` cascade, `usr` | The `--to` target, when given |
| `callgraph_run_edge` | `(run_id, source_id, destination_id, kind, position, file_id, offset) PK` -> `callgraph_run` cascade, `depth`, `cycle CHECK IN (0,1)` | Only the `relation_site` rows actually reached; `kind` is `1` (`Calls`) or `18` (`DispatchCalls`) |
| `callgraph_run_frontier` | `run_id` -> `callgraph_run` cascade, `symbol_id`, `reason` | Discovered-but-not-admitted endpoints (`max_depth`, `max_nodes`, `max_edges`, `time_limit`, `cancelled`) |
| `callgraph_run_recovery` | `(run_id, tu_file_id) PK` -> `callgraph_run` cascade, `outcome CHECK IN ('attempted','failed','reused','suppressed')`, `diagnostic` | One row per translation unit a `--recover-missing` run attempted |

See [call graphs](../04-call-graphs/01-overview.md) for what a run means and
[entries, runs, and status](../04-call-graphs/03-entries-runs-and-status.md#runs-identity-and-status)
for the per-status table and how to read a run back.

## Project (configuration) database

Owns `FileId` values, repositories, checkout clones, components, directories,
files, and compile configuration. Has no `user_version` contract of its own -
the SDK validates it by required-table shape instead. It does have its own,
unrelated `project_registry.schema_version`.

| Table | Key columns | Meaning |
|---|---|---|
| `semantic_universe` | `id PK`, `key UNIQUE`, `name`, `policy` default `'explicit'` | Row `1` = `'legacy'`/`'Legacy single-workspace universe'`/`'legacy'`, always present |
| `repository` | `id PK`, `name UNIQUE`, `kind` default `'repo'`, `remote_url`, `active_clone_id`, `semantic_universe_id` -> `semantic_universe(id)` | |
| `clone` | `id PK`, `repository_id` -> `repository(id)` cascade, `path UNIQUE`, `label` | A registered checkout |
| `component` | `id PK`, `name`, `path`, `kind` default `'repo'`, `version`, `repository_id` -> `repository(id)`, `semantic_universe_id` -> `semantic_universe(id)`, `UNIQUE(repository_id, path)` | |
| `directory` | `id PK`, `component_id` -> `component(id)` cascade, `path`, `UNIQUE(component_id, path)` | |
| `file` | `id PK CHECK(id >= 1)`, `directory_id` -> `directory(id)` cascade, `name`, `mtime`, `md5`, `compile_options`, `driver`, `working_directory`, `indexed` default `0`, `indexed_at`, `facts_db`, `git_commit`, `args_overridden` default `0`, `UNIQUE(directory_id, name)` | `id` is the FileId the facts database's `SymbolId`s reference |
| `project_registry` | `id PK CHECK(id = 1)`, `complete` default `0`, `fingerprint` default `''`, `file_count` default `0`, `schema_version` default `0` | Single row; `import` writes it, extraction consumes it as the completeness gate |
| `matched_symbol_index` | `(usr, file_id) PK` -> `file(id)` cascade, `qualified_name`, `kind`; index `matched_symbol_name(qualified_name, kind)` | Added at `project_registry.schema_version = 1`. Populated **only** by `match`, never by `extract`. Exactly four data columns - no more are ever added to preserve the match-only contract. |

`matched_symbol_index` is a match-only discovery index: a hit is a real
candidate, but a miss never proves a symbol is absent from source, and an
index row never proves a definition, body, calls, or a complete call graph
is available.

`file.indexed`, `indexed_at`, `mtime`, `facts_db`, and `git_commit` are the
per-file index state a real `extract` run writes back once it commits that
file's facts: `indexed` is `1` and `indexed_at` is the UTC extraction time
(`YYYY-MM-DDTHH:MM:SSZ`); `mtime` is the file's last-write time (seconds
since epoch, read by `stat` at that same moment); `facts_db` is the
absolute, lexically-normalized path of the facts database it was extracted
into; `git_commit` is the 40-hex `HEAD` commit of the git repository
tracking the file, or `NULL` when the file is untracked, ignored, outside
any repository, or `HEAD` is unborn. `md5` stays unused. A later `extract`
reads these back to decide whether the file is still up to date - see
[skipping up-to-date sources](../03-extracting-facts/01-extract.md#skipping-up-to-date-sources).
`facts_db`/`git_commit` are additive columns (`ALTER TABLE file ADD COLUMN`
inside the existing schema version); a registry a read-only handle opened
before a writer added them reads back as "not indexed" instead of failing.

## Schema version history (facts `user_version`)

| Version | Added | Notes |
|---|---|---|
| 8 -> 9 | `callable_return_type` table; `relation` `kind=21` (`ReturnType`) | Functions, methods, lambda call operators, and function-object call operators get a return-type row; constructors/destructors never do |
| 9 -> 10 | `symbol.is_volatile` | Previously-unused bit; no identity changes |
| 10 -> 11 | `facts_project_provenance`; `callgraph_entry`; `callgraph_external_reference`; `callgraph_unresolved_site` | See [call-graph-entries.md](../../call-graph-entries.md) |
| 11 -> 12 | `callgraph_run`, `callgraph_run_root`, `callgraph_run_target`, `callgraph_run_edge`, `callgraph_run_frontier`, `callgraph_run_recovery` | Append-only run history. A schema-11 store gains these tables the next time any command opens it read/write, even before recording a symbol or a run. |

Fresh databases on main are created directly at `user_version = 12`. Schema
`13` (opt-in expression/field-access/source-region evidence) exists only on
an unmerged branch - see [in-flight F-013](05-in-flight-f-013.md).

## Python SDK schema support

The Python SDK (`facts-tool-query`) accepts facts `user_version` **10, 11, or
12** (`python/src/facts_tool/schema.py`). Schema 12 additionally requires all
six `callgraph_run*` tables and their documented columns, else it raises
`E_SCHEMA`. The project database has no version gate, only required-table and
-column shape checks.

`python/docs/databases.md` and `python/docs/troubleshooting.md` currently
say only `user_version=10` (and `11`) are supported, and that `E_SCHEMA`
reports a version "other than 10." **This text is stale** - verified
directly against `schema.py`, which checks `version not in (10, 11, 12)`.
Trust `schema.py` and this chapter over that prose.

## Pairing and provenance

A successful `open_codebase` still reports `pairing="unverifiable"`: numeric
`FileId` overlap between the facts and project databases cannot prove they
came from the same indexing run. Schema-11+ facts stores carry
`facts_project_provenance` as stronger identity evidence the native writer
uses to reject incompatible pairs going forward, but migrating an *old*
facts database up to schema 11+ does not retroactively establish that
provenance for data extracted before the migration. If pairing cannot be
proved, write to a new facts file and re-extract every source rather than
trusting the existing pair.

`FileId 0` is reserved for compiler-provided symbols (no source path) - both
fixed primitive-type IDs (`BuiltinType::Kind + 1`) and dynamically-allocated
compiler-symbol IDs live there. See
[locationless callables](04-limitations-and-known-issues.md).

## Supported access only

- **Read through the public Python SDK only**: use public symbol, relation,
  project-view, and persisted-run APIs. Never inspect tables or construct
  joins yourself, including for diagnostics. Report missing public API
  support as a capability gap.
- **Never write directly**: any table in either database. The native writer
  is the only supported writer; it enforces invalidation-on-mutation,
  append-only run history, and foreign-key integrity that a hand-written
  statement will not replicate. Use `facts-tool` (extract/import/match/
  analyse/repo/component/dir/file) or, for programmatic writes, there is no
  supported write path at all outside the native CLI - the Python SDK is
  read-only by design (`mode=ro` URI, connection-local `query_only`).
