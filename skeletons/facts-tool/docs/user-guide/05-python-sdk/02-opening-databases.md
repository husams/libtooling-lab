# Opening databases

Every SDK session starts by pairing a facts database with a project
database through `open_codebase`. This chapter covers what that call
validates, exactly what fails and why, and what read-only guarantees you
get in return.

## `open_codebase`

```python
def open_codebase(
    *,
    facts_db: str | os.PathLike[str],
    project_db: str | os.PathLike[str],
    budgets: Budgets | None = None,
) -> CodeBase
```

Both `facts_db` and `project_db` are mandatory keyword arguments. Both must
name existing, different physical files. `open_codebase` enforces read-only
access internally; agents must never open their own database connections or
issue SQL, including for diagnostics. The SDK creates no path, table,
migration, backfill, journal setting, or persistent side file, and honors
writer locks from any process still extracting into the same store.

```python
from facts_tool import open_codebase

with open_codebase(facts_db="facts.sqlite", project_db="project.sqlite") as cb:
    print(cb.provenance.facts.schema.user_version, cb.provenance.project.schema.user_version)
```

Real output against a schema-12 facts database paired with its project
database:

```text
12 0
```

(The project database has no `user_version` contract; it validates by
required-table shape instead, so its reported `user_version` is simply
whatever SQLite's default is - `0` here.)

## `CodeBase`

`open_codebase` returns a `CodeBase`, the SDK's session object:

| Attribute / method | Meaning |
|---|---|
| `.executor` | the `Executor` that runs query plans - see [03-query-model.md](03-query-model.md) |
| `.provenance` | a `PairProvenance` describing both database identities |
| `.graph` | a `GraphQuery` for typed navigation - see [05-relations-and-graph-queries.md](05-relations-and-graph-queries.md) |
| `.callgraphs` | a `CallGraphReader` for persisted call-graph runs - see [06-persisted-callgraph-runs.md](06-persisted-callgraph-runs.md) |
| `.get(ref)` / `.find(ref)` / `.query(ref=None)` | thin delegators to `.graph.get` / `.graph.find` / `.graph.query` |
| `.close()` | idempotent; closes both connections |

`CodeBase` is a context manager. `__exit__` always calls `close()`, and
`open_codebase` itself closes both connections and re-raises if anything
fails *during* opening (schema inspection, pairing validation) after the
raw connections were already made - you never leak a file handle from a
failed open.

## Supported schema versions

Facts database schema support is governed by `schema.py`, which is more
current than any narrative description elsewhere: facts `user_version` **10,
11, or 12** are accepted. Anything else fails before any table shape is even
checked. Schema **12** additionally requires all six `callgraph_run*` tables
with their documented columns; a schema-12-shaped database missing one of
those tables or columns fails `E_SCHEMA` even though its `user_version` is
otherwise acceptable. The project database has no version gate at all -
only required-table and required-column shape.

> Some narrative docs shipped alongside the SDK (`docs/databases.md`,
> `docs/troubleshooting.md`, `docs/quickstart.md`) still say only schema 10
> and 11 are supported, or that any version other than 10 fails. That text
> predates schema 12 support. `schema.py`'s live behavior - verified below -
> is the current contract: 10, 11, and 12 are all accepted.

### The exact error text

An unsupported `user_version` (verified against a hand-built facts database
with `PRAGMA user_version=5;` and empty `symbol`/`relation` tables):

```text
E_SCHEMA: facts schema user_version 5 is unsupported; need 10, 11, or 12
```

A schema-12-shaped database that is missing one or more of the six
`callgraph_run*` tables fails with the same code and a message built from
the exact missing table names. Verified by copying a real schema-12 facts
database and dropping `callgraph_run_edge`:

```text
E_SCHEMA: unsupported schema 12 layout; lacks callgraph tables: callgraph_run_edge
```

A schema-12 database whose `callgraph_run*` tables are all present but is
missing a required column on one of them fails the same way, with the table
and column named directly. Verified by dropping the `cycle` column from
`callgraph_run_edge`:

```text
E_SCHEMA: facts.callgraph_run_edge lacks columns: cycle
```

The required-column set is the *persisted* column list, not the SDK's model
fields: `callgraph_run_edge` must carry `run_id`, `source_id`,
`destination_id`, `kind`, `position`, `file_id`, `offset`, `depth`, and
`cycle`. A model attribute such as `CallGraphEdge.semantic_kind` is derived
by the reader and is never a stored column, so it can never appear in this
message.

A required table missing from either database (independent of the schema
12 gate) is instead `E_DATABASE_ROLE`, e.g. a fully wrong-role facts
argument (verified by swapping the facts/project arguments):

```text
E_DATABASE_ROLE: facts database lacks tables: callable_return_type, definition, enumeration, enumerator, include_dependency, parameter, parameter_default, relation, relation_site, symbol, template_argument, template_parameter, variable_initializer
```

### Other open-time failures, verified live

Passing the same path for both `facts_db` and `project_db`:

```text
E_DATABASE_ROLE: facts and project databases must be different files
```

A missing path:

```text
E_DATABASE: facts database does not exist: <path>
```

A facts database paired with an unrelated project database (disjoint
`FileId` universe - `pairing.validate_pair` requires the project database to
carry a complete, fingerprinted `project_registry` row and checks that every
`FileId` used by `symbol`, `definition`, `relation_site`, and
`include_dependency` in the facts database exists in the project database's
`file` table):

```text
E_DATABASE_PAIR: project database lacks FileIds: [2]
```

## Pairing and provenance

A **successful** open still reports `provenance.pairing == "unverifiable"`.
Numeric `FileId` overlap between two databases cannot by itself prove they
came from the same indexing run - only the native writer's own extraction
history can prove that. `PairProvenance` (and every `Result` and
`CallGraphRun`, via `.to_dict()`'s `"provenance"` key) carries:

```python
PairProvenance(
    facts=DatabaseIdentity(path, device, inode, size, mtime_ns, schema=SchemaIdentity(...)),
    project=DatabaseIdentity(path, device, inode, size, mtime_ns, schema=SchemaIdentity(...)),
    pairing="unverifiable",
)
```

`SchemaIdentity` carries `role`, `user_version`, `schema_version` (SQLite's
own `PRAGMA schema_version`), and the full sorted table-name tuple. This is
provenance in the sense of *recorded evidence about what you opened*, not a
promise of an atomic cross-database snapshot.

## Read-only guarantees

- Both connections are opened with SQLite URI `mode=ro` plus connection-local
  `PRAGMA query_only=ON`.
- The SDK never creates a path, table, migration, backfill, journal setting,
  or side file.
- `close()` is idempotent - calling it twice, or via both an explicit call
  and context-manager exit, is safe.
- Any exception raised while opening (schema inspection, pairing
  validation) closes both raw connections before re-raising, so a failed
  `open_codebase` call never leaks file descriptors.
- Query sessions never migrate a database; if `E_SCHEMA` fires, the fix is
  to re-run the native `facts-tool` workflow, not to retry the SDK call
  with different arguments.

## `SymbolId` and identifiers

Every symbol identity the SDK hands back is a `SymbolId`
(`frozen, order=True` dataclass of `(file_id, index)`, each an unsigned
32-bit half):

```python
from facts_tool import SymbolId

sid = SymbolId(file_id=2, index=20)
print(sid.packed, sid.sqlite, sid.to_dict())
print(SymbolId.unpack(sid.packed) == sid)
```

```text
8589934612 8589934612 {'packed': '8589934612', 'file_id': 2, 'index': 20}
True
```

`.packed` is `(file_id << 32) | index`. `.sqlite` folds the packed value
back into SQLite's signed 64-bit domain when the top bit would otherwise
overflow a signed 64-bit integer:

```python
big = SymbolId(file_id=(1 << 31), index=0)
print(big.packed, big.sqlite)
```

```text
9223372036854775808 -9223372036854775808
```

Constructing a `SymbolId` with an out-of-range half raises `E_IDENTITY`:

```python
SymbolId(file_id=1 << 40, index=1)
```

```text
E_IDENTITY: symbol identity halves must be unsigned 32-bit
```

JSON export (`Result.to_dict()`/`.to_json()`, and every `CallGraph*`
model's `.to_dict()`) stringifies any integer whose magnitude exceeds
`2**53 - 1`, so large packed identities survive round-tripping through
JavaScript's float-based JSON number type without loss.

Continue to [03-query-model.md](03-query-model.md) for the declarative
query language itself.
