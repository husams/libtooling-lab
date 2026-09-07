# How to build a call graph

Use the native executable built from this checkout. Confirm its configuration
with `facts-tool config show`, and use `symbol find` to disambiguate the exact
qualified name or USR (see [symbol search](how-to-search-symbol.md)).

## Render existing evidence

```sh
facts-tool analyse call-graph --conf project.db --facts facts.db \
  --function main --format mermaid --output main-callgraph.mmd
```

This reads existing facts and atomically replaces the diagram file. Omit
`--output` to write stdout. Use `--format text` (the default) or `--format json`
for one final text or JSON result. A qualified-name collision is a usage error;
select the reported exact USR instead. A missing root never fabricates a diagram.

## Recover missing evidence explicitly

```sh
facts-tool analyse call-graph --conf project.db --facts facts.db \
  --function main --recover-missing --format mermaid --output main-callgraph.mmd
```

Recovery can write facts using imported translation-unit commands. Mermaid
file output first publishes a visibly partial diagram, before recovery starts,
then replaces it atomically as graph evidence changes and when recovery ends.
Readers always see a whole file. A recovery failure exits 1 and retains a valid
partial diagram; inspect its metadata and stderr for attempted units and errors.
Text and JSON have one final document, with progress confined to stderr.

The diagram preserves shared nodes and cycles. Labels identify native relation
and callable semantics, call sites, and unresolved or external boundaries. Its
JSON header comment retains root USRs, pair paths, source locations, selected
scope, budgets, exclusions, truncation frontiers, and recovery evidence.

Default traversal is forward across all registered components with no automatic
limits. Explicit options include `--component NAME`, `--calls-scope
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
  --facts build/graph-recipe/facts.db --function main --recover-missing \
  --format mermaid --output build/graph-recipe/main.mmd
```

Expected stored edges: `main -> s025_bridge -> s025_leaf`. The registered
`native_graph_rendering.feature` additionally exercises discovery and recovery
across independently imported app/library components, failures, and live
initial/final artifact publication.

A matched symbol is discovery evidence, not proof that its body or calls were
extracted. Stored traversal completion, extraction coverage, freshness,
unresolved calls, and recovery failures remain separate statements. A budget
stop is incomplete traversal; external boundaries are not fabricated callees.

If configuration or pair validation fails, correct the selected project/facts
paths first. If recovery cannot find or compile a definition, inspect its
reported translation-unit path, imported compiler arguments, and diagnostic;
retain the partial graph rather than claiming complete source coverage.
