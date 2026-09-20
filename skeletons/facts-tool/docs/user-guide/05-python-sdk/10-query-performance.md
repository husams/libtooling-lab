# Lazy queries and performance

Plan queries are lazy by default. `cb.executor.run(plan)` and
`cb.query(...).run()` validate the plan and return a `Result`; iteration
executes it. Simple enumeration, filtering, projection, and limits can yield
rows as they are decoded. Use this mode to process large results incrementally.

## Stream rows while the codebase is open

```python
from facts_tool import Budgets, open_codebase
from facts_tool.queryplan import codebase, eq, nodes, select, start

with open_codebase(
    facts_db="facts.sqlite",
    project_db="project.sqlite",
    budgets=Budgets(enumeration=100_000, result_cap=100_000),
) as cb:
    query = (
        start(codebase())
        | nodes(eq("kind", "function"))
        | select(("qualified_name", "file", "line"))
    )
    result = cb.executor.run(query.plan)
    for row in result:
        print(row["qualified_name"], row["file"], row["line"])
    if result.truncated:
        print("A budget was reached; this is an incomplete result.")
```

Lazy iteration preserves the budgets. Defaults remain 10,000 enumerated rows,
10,000 traversal states, and 1,000 returned results. Raise both enumeration
and result limits when a full scan needs more rows; the example above is still
bounded at 100,000. Filtering uses the bounded enumeration window, so a
selective predicate does not automatically search beyond that window.

Fluent queries also support direct iteration. Unprojected symbol queries yield
typed entities; projected queries yield rows:

```python
with open_codebase(facts_db="facts.sqlite", project_db="project.sqlite") as cb:
    for entity in cb.query().nodes(eq("kind", "function")):
        print(entity.qualified_name)
```

A generator wrapper can own the session and yield rows to its caller. Close a
retained iterator explicitly when stopping early, so its cursor and session
are released promptly:

```python
from contextlib import closing

def iter_function_rows(facts_db, project_db):
    with open_codebase(
        facts_db=facts_db,
        project_db=project_db,
        budgets=Budgets(enumeration=100_000, result_cap=100_000),
    ) as cb:
        query = cb.query().nodes(eq("kind", "function")).select(("qualified_name",))
        result = query.run()
        yield from result
        if result.truncated:
            raise RuntimeError("Function enumeration reached its budget")

with closing(iter_function_rows("facts.sqlite", "project.sqlite")) as rows:
    print(next(rows, None))
```

The completeness check runs only if the generator is exhausted. Stopping
after the first row does not establish whether the complete query is truncated.

## Choose eager execution when needed

Set the default on the session, or override it for one query:

```python
with open_codebase(
    facts_db="facts.sqlite", project_db="project.sqlite", lazy=False,
) as cb:
    query = start(codebase()) | nodes()
    eager = cb.executor.run(query.plan)              # inherits lazy=False
    streamed = cb.executor.run(query.plan, lazy=True)
    for row in streamed:
        print(row["qualified_name"])

print(len(eager))  # cached rows remain usable after the session closes
```

Both `cb.executor.run(plan, lazy=False)` and `cb.query().nodes().run(lazy=False)`
execute immediately and retain their bounded results. This preserves the
previous execution timing. Fluent `all()` and `names()` also collect eagerly.

## Materialization, repeated reads, and errors

- A plain `for` loop does not retain every row. Keep the codebase open until
  the loop or materialization finishes.
- `values`, the matching `nodes`/`rows`/`paths` property, `len(result)`,
  `list(result)`, `materialize()`, `to_dict()`, and `to_json()` collect and
  cache the result.
- Reading `truncated`, `partial`, `unknown`, `cursor`, or `scalar` before
  execution finishes also completes and materializes it. Inspect completion
  metadata after exhausting the iterator to avoid that allocation.
- Each unmaterialized iteration executes the query again. Materialization
  after streaming also reruns it and caches that full pass. Use eager mode
  or `materialize()` when repeated reads must reuse the same collected rows.
- Invalid plans still fail at `run()`. Row-dependent errors, such as a
  missing symbol or unknown evidence, occur during consumption in lazy mode.
  Put exception handling around the loop, or use eager execution.

Sorting and distinct stages buffer intermediate rows; counts consume their
input before producing a scalar. Graph traversal, paths, and set operations
defer execution but still buffer bounded results. Specialized evidence and
persisted call-graph APIs retain their eager page behavior. Lazy execution
therefore does not imply that every query has constant memory use.

Resolved file paths use a bounded cache within the codebase. Close and reopen
the codebase after changing file locations, components, or active clones in
the project registry to refresh these paths.

## Which queries use indexes?

The earlier SDK decoded every symbol before filtering exact references or
IDs, bypassing useful indexes and resolving file paths for unrelated rows.
It now passes those lookups to SQLite and decodes only matching symbols.
Graph navigation hydrates reached symbols without loading the whole
symbol table. No database migration is required.

| Operation | Access path |
|---|---|
| Exact USR | `idx_symbol_unique_usr` |
| Exact qualified name | `idx_symbol_qualified_name` |
| Symbol ID | Integer primary key |
| Outbound relation | Relation primary key beginning with `source_id` |
| Inbound relation | `idx_relation_destination` |
| Kind filter | Scan; no dedicated kind index |
| Short-name fallback | Scan IDs/names, then decode matches |

Supported adjacent symbol `nodes`/`where` predicates on stored IDs, USRs,
qualified names, and kinds are pushed into SQL when comparison semantics
match. A lightweight ID probe first establishes the enumeration window;
fluent filtering therefore still has work beyond the indexed match. Other
predicates run in Python. Relation ordering may use a temporary B-tree.

Prefer an exact USR when overload identity matters, or a qualified name when
all matching overloads are wanted. See [The query model](03-query-model.md)
for reference resolution and [Results](../../../python/docs/results.md)
for budgets and continuation cursors.

## Measured on facts-tool's own code

The benchmark compared baseline commit `4eef64a` with the updated SDK in
`a595a3d`, using identical native-extracted databases from all 273 production
C++ translation units under `skeletons/facts-tool/src`. Extraction succeeded
for all 273 files. The corpus contains 18,524 symbols, including 8,056 external
stubs, and 40,509 relations.

Times are milliseconds, median of seven warm runs. Database opening,
extraction, SQL tracing, and memory instrumentation are excluded. Every
query consumes its full result except the explicitly partial first-row case.

| Query | Rows consumed | Previous SDK | Updated eager | Updated lazy |
|---|---:|---:|---:|---:|
| Exact USR lookup | 1 | 711.37 | 0.081 | 0.066 |
| Exact qualified-name lookup | 1 | 677.52 | 0.089 | 0.084 |
| Fluent qualified-name filter | 1 | 684.98 | 1.190 | 1.255 |
| One-hop calls | 77 | 1,359.99 | 1.605 | 1.662 |
| Function-kind filter | 2,155 | 685.33 | 40.61 | 43.54 |
| Iterate every symbol | 18,524 | 669.43 | 343.81 | 319.24 |
| Consume only the first symbol | 1 | 682.83 | 345.80 | 1.853 |

For full symbol iteration, peak additional Python allocation after warm-up
was 47.447 MiB before, 44.491 MiB with updated eager mode, and 27.25 KiB with
lazy mode. This separate `tracemalloc` measurement excludes retained caches
and SQLite/native memory. First-row latency during the full scan was
667.21 ms before, 340.35 ms eager, and 1.891 ms lazy.

The exact-name query targeted `facts::Storage::loadSymbolRow`; the calls
query started at `facts::(anonymous namespace)::prepareCommands`. SQL tracing
recorded one symbol scan and 18,513 project/file-path queries for the old
exact-name lookup. The updated lookup issued two indexed facts queries and
no project queries with a warmed path cache.

All three modes produced identical ordered full-row hashes and counts.
Enumeration, result, and traversal budgets were raised to 59,034 for this
comparison; all fully consumed results were untruncated. The environment
was CPython 3.12.14, SQLite 3.53.1, Linux x86-64, on an AMD EPYC 9V74.
Opening the updated codebase separately took about 8.6 ms. These measurements
describe this corpus and workload; cache state, file count, graph fan-out,
and query shape affect other workloads.

For the smaller, reproducible synthetic benchmark shipped with the SDK and
the detailed query plans, see [Query performance](../../../python/docs/query-performance.md).
