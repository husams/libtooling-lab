# How to build a call graph

Use the native executable built from this checkout. Confirm its configuration
with `facts-tool config show`, and use `symbol find` to disambiguate the exact
qualified name or USR (see [symbol search](how-to-search-symbol.md)).

## Run the graph

```sh
facts-tool analyse call-graph --conf project.db --facts facts.db \
  --function main
```

This reads existing facts and traverses. It prints exactly one completion
line on stdout:

```text
facts-tool: call graph run <run_id> <status>
```

`<status>` is `complete`, `truncated`, `cancelled`, `recovery-failed`, or
`failed`. No text listing, JSON, or diagram file is produced; the traversal is
persisted to the facts database as one append-only run identified by
`<run_id>` (see [the run contract](../../../../docs/call-graph.md)). A
qualified-name collision is a usage error; select the reported exact USR
instead. A missing root writes no run.

## Recover missing evidence explicitly

```sh
facts-tool analyse call-graph --conf project.db --facts facts.db \
  --function main --recover-missing
```

Recovery can write facts using imported translation-unit commands. Every
attempted translation unit is recorded in `callgraph_run_recovery`; a failure
completes the run with status `recovery-failed`, prints one stderr summary
line, and exits 1, while keeping the edges reached before the failure. See
[recovery](../../../../docs/call-graph-recovery.md) for the outcome table.

Default traversal is forward across all registered components with no
automatic limits. Explicit options include `--component NAME`, `--calls-scope
all|project|library`, `--max-depth N`, `--max-nodes N`, `--max-edges N`, and
`--time-limit-ms N`. Reverse queries use `--direction callers`; paths use
`--to TARGET --path-mode shortest|all-simple`. See
[the native reference](../../../../docs/call-graph.md).

## Fixture-backed recipe

From the facts-tool project directory, after building `build/facts-tool`:

```sh
mkdir -p build/graph-recipe
build/facts-tool import --conf build/graph-recipe/project.db \
  --extra-arg=-std=c++23 tests/fixtures/e2e/s025_workflow.cpp
build/facts-tool extract --conf build/graph-recipe/project.db \
  --output build/graph-recipe/facts.db tests/fixtures/e2e/s025_workflow.cpp
build/facts-tool analyse call-graph --conf build/graph-recipe/project.db \
  --facts build/graph-recipe/facts.db --function main --recover-missing
```

This prints `facts-tool: call graph run 1 complete` (the run ID may differ if
the facts database already has runs). Read the run back with Python:

```sh
python3 -c "
import sqlite3
conn = sqlite3.connect('build/graph-recipe/facts.db')
run_id = conn.execute('SELECT max(run_id) FROM callgraph_run').fetchone()[0]
rows = conn.execute('''
    SELECT s1.qualified_name, s2.qualified_name
    FROM callgraph_run_edge e
    JOIN symbol s1 ON s1.id = e.source_id
    JOIN symbol s2 ON s2.id = e.destination_id
    WHERE e.run_id = ?
    ORDER BY e.depth
''', (run_id,)).fetchall()
print(rows)
"
```

Expected edges: `main -> s025_bridge` and `s025_bridge -> s025_leaf`. The
registered `native_graph_rendering.feature` additionally exercises discovery
and recovery across independently imported app/library components, failures,
and the persisted run contract.

A matched symbol is discovery evidence, not proof that its body or calls were
extracted. Stored traversal completion, extraction coverage, freshness,
unresolved calls, and recovery failures remain separate statements. A budget
stop is a truncated run, recorded with its reason; external boundaries are
not fabricated callees.

If configuration or pair validation fails, correct the selected project/facts
paths first. If recovery cannot find or compile a definition, inspect
`callgraph_run_recovery`'s recorded translation-unit and diagnostic; retain
the partial run rather than claiming complete source coverage.
