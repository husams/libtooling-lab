# Query performance and index audit

The primary bottleneck was loading and decoding every symbol before selecting
matching results. `lookup_symbol()` compared USRs and names in Python even
though SQLite already indexed both columns. `load_symbols(ids=...)` likewise
filtered only after loading everything. Every graph traversal expanded its
symbol lookup dictionary from the entire database, repeating that work at
each hop. Decoding a symbol also executed a project path query per symbol.

Exact USR and qualified-name lookup now uses parameterized SQL predicates.
Requested symbol IDs use primary-key lookups in batches of at most 500 IDs,
including conversion between unsigned SDK identities and signed SQLite keys.
Graph traversal decodes only its destination symbols and preserves traversal
order. File paths are cached by FileId for the lifetime of a codebase.
Close and reopen the codebase after changing project file locations, components,
or active clones so subsequent results resolve paths using the updated registry.
Short-name lookup retains its previous matching rules: when no exact USR or
qualified name matches, it scans only IDs/names, then decodes matching rows.

## SQLite query plans

These plans were checked with `EXPLAIN QUERY PLAN` against `src/storage/Schema.h`.

| Operation | SQLite plan |
| --- | --- |
| Previous symbol enumeration | `SCAN s` |
| Exact USR lookup | `SEARCH s USING INDEX idx_symbol_unique_usr (usr=?)` |
| Exact qualified name | `SEARCH s USING INDEX idx_symbol_qualified_name (qualified_name=?)` |
| Symbol ID lookup | `SEARCH symbol USING INTEGER PRIMARY KEY (rowid=?)` |
| Outbound stored relation | `SEARCH relation USING PRIMARY KEY (source_id=?)` |
| Inbound stored relation | `SEARCH relation USING COVERING INDEX idx_relation_destination (destination_id=? AND kind=?)` |

No database migration or writable SDK connection is needed. The existing
indexes become useful once predicates reach SQLite. This does not mean every
query is indexed: kind filters have no dedicated index, derived short names
still scan, relation ordering can use a
temporary B-tree, and reverse include lookup scans the include index. Sorting,
ranking, and some graph/set operations can still require buffered results.

Adjacent symbol `nodes`/`where` predicates on stored IDs, USRs, qualified names,
and kinds are pushed into SQL when their Python comparison semantics can be
preserved. A lightweight ID probe bounds the enumeration window before filtering,
so an indexed filter keeps the existing budget and cursor behavior. Other
predicates stream through Python. No rows are decoded just to find the window.

## Reproducible synthetic measurement

`scripts/benchmark_symbol_queries.py` builds 100,000 symbols using the native
schema, all in one project file, with one call edge. It times direct exact-name
lookup and one-hop navigation, excluding database construction and opening.
Results below are medians of three runs in the same container, against baseline
commit `4eef64a` and the updated SDK. The project path cache is warm.

| Operation | Baseline | Updated |
| --- | ---: | ---: |
| Qualified-name lookup | 3.529 s | 0.062 ms |
| One-hop traversal | 3.445 s | 0.073 ms |
| Project SQL statements per lookup/traversal | 100,000 | 0 |
| Symbols decoded per lookup/traversal | 100,000 | 1 |

These are focused synthetic measurements, not production latency predictions.
Disk I/O, distinct file count, graph fan-out, and the predicate shape affect
real workloads. The deterministic regression checks verify index selection
and decoded row counts without relying on timing thresholds.

From the Python SDK directory:

```bash
uv run python scripts/benchmark_symbol_queries.py
uv run python scripts/benchmark_symbol_queries.py --symbols 100000 --repeat 3
```

To compare another checkout, pass its SDK source directory:

```bash
uv run python scripts/benchmark_symbol_queries.py \
  --sdk-root /path/to/baseline/skeletons/facts-tool/python/src
```
