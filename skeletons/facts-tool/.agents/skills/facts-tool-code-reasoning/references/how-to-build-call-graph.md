# How to build a call graph

Use the native executable built from this checkout. Confirm its configuration
with `facts-tool config show`, and use `symbol find` to disambiguate the exact
qualified name or USR (see [symbol search](how-to-search-symbol.md)).

## Run the graph

```sh
facts-tool analyse call-graph --conf project.db --facts facts.db \
  --function main
```

This reads existing facts and traverses. It prints exactly one completion line
on stdout:

```text
facts-tool: call graph run <run_id> <status>
```

`<status>` is `complete`, `truncated`, `cancelled`, `recovery-failed`, or
`failed`. No text listing, JSON, or diagram file is produced; the traversal is
persisted as one append-only run identified by `<run_id>`. A qualified-name
collision is a usage error; select the reported exact USR instead. A missing
root writes no run.

## Recover missing evidence explicitly

```sh
facts-tool analyse call-graph --conf project.db --facts facts.db \
  --function main --recover-missing
```

Recovery can write facts using imported translation-unit commands. A failure
completes the run with status `recovery-failed`, prints one stderr summary line,
and exits 1, while keeping the edges reached before the failure. See
[recovery](../../../../docs/call-graph-recovery.md) for the outcome table.

Default traversal is forward across all registered components with no
automatic limits. Explicit options include `--component NAME`, `--calls-scope
all|project|library`, `--max-depth N`, `--max-nodes N`, `--max-edges N`, and
`--time-limit-ms N`. Reverse queries use `--direction callers`; paths use
`--to TARGET --path-mode shortest|all-simple`. Use the SDK's `target_reached`,
`self_path`, and `path_found` properties for path outcomes.

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

Read the exact persisted run with the installed public SDK:

```python
from facts_tool import open_codebase

with open_codebase(
    facts_db="build/graph-recipe/facts.db",
    project_db="build/graph-recipe/project.db",
) as cb:
    run_id = 1  # parsed from the native completion line
    run = cb.callgraphs.get(run_id)
    assert run.status == "complete"
    for edge in run.edges:
        print(edge.source.qualified_name, "->", edge.target.qualified_name)
```

A matched symbol is discovery evidence, not proof that its body or calls were
extracted. Stored traversal completion, extraction coverage, freshness,
unresolved calls, and recovery failures remain separate statements. A budget
stop is a truncated run, recorded with its reason; external boundaries are not
fabricated callees.

If configuration or pair validation fails, correct the selected project/facts
paths first. If recovery cannot find or compile a definition, inspect the SDK
run's `recovery` entries and diagnostic; retain the partial run rather than
claiming complete source coverage.
