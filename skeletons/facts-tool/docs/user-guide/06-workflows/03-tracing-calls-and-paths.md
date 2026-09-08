# Workflow: Tracing Calls and Paths

## Goal

Build a persisted call-graph run with `analyse call-graph`, read it back
with the Python SDK's run reader, and use it (and live `GraphQuery`
navigation) to answer "what does this function call" and "how does X reach
Y", including a case where the honest answer is "it doesn't, as far as the
extractor could resolve".

## Prerequisites

- A facts database at native schema 12 (produced by `extract`; see
  [Onboarding a Codebase](01-onboarding-a-codebase.md)).
- The Python SDK installed.
- `cb` below is an open `CodeBase` (`open_codebase(facts_db=..., project_db=...)`).

## Steps

### 1. Start with a minimal fixture for a fast, deterministic sanity check

Before pointing `analyse call-graph` at a large codebase, it's worth
running it once against a tiny fixture so you know exactly what a
"found" and a "complete" run look like:

```console
$ facts-tool import --conf mini/project.db --facts mini/facts.db --extra-arg=-std=c++23 \
    tests/fixtures/e2e/s025_workflow.cpp
$ facts-tool extract --conf mini/project.db -o mini/facts.db -v 0 tests/fixtures/e2e/s025_workflow.cpp
facts-tool: 3 symbol(s) recorded from 1 file(s)
$ facts-tool analyse call-graph --conf mini/project.db --facts mini/facts.db --function main
facts-tool: call graph run 1 complete
$ facts-tool analyse call-graph --conf mini/project.db --facts mini/facts.db \
    --function main --to s025_leaf --path-mode all-simple
facts-tool: call graph run 2 complete
```

```python
run1 = cb.callgraphs.get(1)
for e in run1.edges:
    print(e.source.qualified_name, "--", e.semantic_kind, "-->", e.target.qualified_name, e.site)
# main -- Calls --> s025_bridge  .../s025_workflow.cpp line=5
# s025_bridge -- Calls --> s025_leaf  .../s025_workflow.cpp line=3

run2 = cb.callgraphs.get(2)
print(run2.path_outcome, run2.path_found)   # found True
```

**What this tells you:** `analyse call-graph` never prints a graph, only a
one-line completion status; the graph itself lives in the facts database as
an append-only run, read back through `cb.callgraphs`. See
[Overview](../04-call-graphs/01-overview.md) for why.

### 2. Budget truncation and the frontier

```console
$ facts-tool analyse call-graph --conf mini/project.db --facts mini/facts.db --function main --max-depth 1
facts-tool: call graph run 3 truncated
```

```python
run3 = cb.callgraphs.get(3)
# status: truncated  truncation_reason: max_depth
# edges: main -> s025_bridge (depth=1)
# frontier: s025_bridge  reason=max_depth
```

**What this tells you:** the frontier records the node whose *expansion*
was cut off (`s025_bridge`, not the root `main`). A `truncated` run still
exits 0; truncation is a successful-but-incomplete outcome, not an error.
See
[Recovery and Boundaries](../04-call-graphs/04-recovery-and-boundaries.md)
for the full frontier/boundary model.

### 3. `call-graph-entry`: one function's stored evidence

```console
$ facts-tool analyse call-graph-entry --conf mini/project.db --facts mini/facts.db --function main --format json
{"entry_available":true,"graph_node_ref":"4294967298","is_leaf":false,"symbol_id":"4294967298","usr":"c:@F@main#", ...}
$ facts-tool analyse call-graph-entry ... --function s025_leaf --format json
{"entry_available":true,"is_leaf":true, ...}
```

This is a single function's metadata record, not the graph, and it comes
from `extract`'s own committed function-entry generation, independent of
any `analyse call-graph` run. See
[Entries, Runs, and Status](../04-call-graphs/03-entries-runs-and-status.md).

### 4. Move to the real codebase: a budgeted call-graph from `main`

Continuing with the same `project.sqlite`/`facts.sqlite` pair from
[Onboarding a Codebase](01-onboarding-a-codebase.md):

```console
$ time facts-tool analyse call-graph --conf project.sqlite --facts facts.sqlite \
    --function main --max-depth 5 --max-nodes 200 -v 1
facts-tool: call-graph: starting
facts-tool: roots selected
facts-tool: graph traversal
facts-tool: call-graph: complete
facts-tool: call graph run 1 truncated
0.226s total
```

```python
run = cb.callgraphs.get(1)
# status=truncated, truncation_reason=max_depth, edges.total=907, edges.complete=True
main -- Calls --> facts::cli::run                          depth=1
facts::cli::run -- Calls --> facts::cli::(anon)::Parser::Parser  depth=2  site.line=292
facts::cli::run -- Calls --> facts::cli::dispatch                depth=2  site.line=293
facts::cli::(anon)::Parser::Parser -- Calls --> ...::configureExtract  depth=3
# frontier.total=6, e.g. facts::cli::catalogOptions reason=max_depth
```

**What this tells you:** a budgeted run over a real codebase completes in
well under a second, because it's a graph read over facts already
extracted, not a re-compilation. `--max-nodes 200` never became the
binding constraint here; `--max-depth 5` hit first, and 907 *edges* can
exceed the 200-node cap because node budgets count canonical symbol
identities while edge budgets count canonical relation keys.

### 5. Reverse callers, and a path query that comes back "unreachable"

```console
$ facts-tool analyse call-graph ... --function facts::commands::detail::factsSchemaVersion --direction callers
facts-tool: call graph run 2 complete
$ facts-tool analyse call-graph ... --function main --to facts::commands::detail::factsSchemaVersion --path-mode all-simple
facts-tool: call graph run 3 complete
```

```python
run2 = cb.callgraphs.get(2)   # status=complete, edges.total=42
# ...::validate <-- caller of -- factsSchemaVersion            depth=1
# ...::(anon)::analyse <-- caller of -- ...::validateFactPairForRead   depth=2
# ... up through .../facts::cli::dispatch(Command)::<lambda@161:7>::operator() depth=8

run3 = cb.callgraphs.get(3)   # path_outcome=unreachable, path_found=False, 925 edges kept anyway
```

**What this tells you, honestly:** reverse traversal
(`--direction callers`) from `factsSchemaVersion` climbs all the way up
through `facts::cli::dispatch(Command)`'s lambda, which looks reachable
from `main`. Yet the **forward, unbounded path query from `main`** reports
`path_outcome=unreachable` after exploring 925 edges. When a target is
unreachable, the run still completes and keeps every edge the search
actually explored, so a non-empty edge list on a "complete" run is not
proof of reachability. The two directions disagree here because the actual
connection, from `dispatch`'s lambda down to the CLI-parsing helpers, goes
through a call the extractor could not resolve into a static `Calls` or
`DispatchCalls` edge (likely dispatch through `std::variant`/`CLI::App`
callback machinery). **Always check `path_outcome`/`path_found` before
reading a run's edges as a path.**

### 6. Page a run's edges instead of reading all of them at once

```python
run1 = cb.callgraphs.get(1, limit=5)
# edges.total=907, edges.complete=False, edges.next_cursor=5
run1b = cb.callgraphs.get(1, limit=5, cursors={"edges": run1.edges.next_cursor})
# next 5 edges returned
```

Each run's child collections (roots, targets, edges, frontier, recovery)
page independently through `cursors`, so a 907-edge run doesn't have to be
materialized in one call.

### 7. Live `GraphQuery` navigation is a separate thing from a persisted run

`cb.callgraphs` reads a run you already committed with `analyse
call-graph`. `cb.graph`/`cb.get(...).callers()`/`.callees()` are live
relation traversal, computed fresh on every call, and are never a
substitute for, or mislabeled as, a graph run:

This is easiest to see on a tiny demo project where `app::run` calls
`app::save`, which in turn calls `app::persist`:

```python
entry = cb.get("app::run")
print([c.name for c in entry.callees(max_depth=3)])
# ['save', 'persist']
print(cb.query("app::run").relation("calls").names())
# ['save']
```

`callees(max_depth=3)` reaches transitively through `save -> persist`; the
one-hop fluent query stops at `save`. Both are correct for the depth
requested. Use `cb.graph`/typed navigation for a quick, unbounded-lifetime
lookup, and a persisted `analyse call-graph` run when you need budgets,
frontier/boundary reporting, or a record you can page and re-read later
without re-traversing.

## Pitfalls

- **A "complete" forward path run with a non-empty edge list is not proof
  of reachability.** Always check `path_outcome`/`path_found` before
  treating a run's edges as a found path.
- **The frontier records the node whose expansion was cut off, not the
  root.** Don't expect the root symbol to appear in `frontier`.
- **Node budgets count canonical symbol identities; edge budgets count
  canonical relation keys.** A run can hit `--max-depth` well before
  `--max-nodes`, and the edge total can exceed the node cap.
- **Live `GraphQuery` navigation (`callers()`/`callees()`) and persisted
  call-graph runs (`cb.callgraphs`) are independent.** One is computed
  fresh on every call; the other reads a specific run you committed
  earlier. Don't conflate the two when explaining results to someone else.

## Where to go next

- [Overview](../04-call-graphs/01-overview.md) and
  [Generating a Call Graph](../04-call-graphs/02-generating-a-call-graph.md)
  for the full native CLI surface (`--direction`, `--path-mode`,
  `--calls-scope`, `--component`, every budget flag).
- [Recovery and Boundaries](../04-call-graphs/04-recovery-and-boundaries.md)
  for `--recover-missing` and the frontier/boundary model in depth.
- [Persisted Call-Graph Runs](../05-python-sdk/06-persisted-callgraph-runs.md)
  for the full `CallGraphReader` API.
- [Relations and Graph Queries](../05-python-sdk/05-relations-and-graph-queries.md)
  for `GraphQuery`, `path()`, and witness reconstruction.
- [Problem Investigation](04-problem-investigation.md) to use these same
  tools to trace a real function's callers up to entry points.
