# Contextual call graphs

Extraction records `Calls` for statically selected callees, `Overrides` from a
derived method to its base declaration, and conservative `DispatchCalls` for
virtual targets. Each site keeps its source location. A proven concrete
by-value receiver stores its type with `exact` certainty; pointer, reference,
implicit, and otherwise unproven receivers store no type and use `possible`.
No `unknown` certainty is persisted.

The extractor builds Clang's translation-unit call graph once after ordinary
body extraction. `CallGraphVisitor` only orchestrates focused call-site,
receiver, and override extractors. Symbols are joined by canonical USR, so a
declaration-only callee can become the same definition-backed symbol when a
later translation unit supplies its body. Constructor bodies contribute their
calls. Constructor invocations and explicit or frontend-resolved cleanup
actions also contribute `Calls` with their source sites; existing `Construct*`
object/type relations remain unchanged. Cleanup edges are marked implicit for
automatic, temporary, and delete-triggered destruction. Lambda invocations
target the lambda's owned call-operator symbol.

## Querying

Use [function entries](call-graph-entries.md) to distinguish committed body
generation, missing entries, generated leaves, and retained external targets.
Use [missing-evidence recovery](call-graph-recovery.md) for the explicit
`--recover-missing` request across registered components.

Select one root by qualified name or USR:

```text
facts-tool analyse call-graph -f facts.db -c project.db --function app::run
```

Or list every definition-backed root with calls:

```text
facts-tool analyse call-graph -f facts.db -c project.db --all
```

Traversal has no application cap by default: it continues across every
registered component and available library edge until a cycle, reused context,
or external symbol provides a semantic stop. Optional request-only controls are:

- repeatable `--component NAME`, selecting the named component union;
- `--calls-scope all|project|library`, defaulting to `all`;
- positive `--max-depth`, `--max-nodes`, `--max-edges`, and
  `--time-limit-ms` budgets.

Component and project/library filters require the matching project catalog.
They cut at excluded endpoints, report observed boundary identities, and never
walk through an excluded node to reconnect a permitted one. Selected roots are
subject to the same endpoint filters, so an out-of-scope root is reported as
excluded and `--calls-scope library` needs a library-side root. Node budgets
count canonical symbol identities, edge budgets count canonical relation keys,
depth counts call-edge hops, and time uses a monotonic clock. The root counts
as one node. An exact-depth leaf with no qualifying outgoing edge is complete.

Every reached budget records its truncation reason on the persisted run and
in `callgraph_run_frontier`. SIGINT emits a coherent partial result when
possible, records status `cancelled`, and exits 130. Filters, counters,
timers, and cancellation state are released at process exit; neither the
facts database nor project catalog is used as a result cache.

Structural-budget frontier rows are discovered endpoints that were not
admitted or expanded. For time limits and cancellation, the frontier also
includes the admitted node whose expansion stopped and every remaining
eligible selected root. These controls also apply to reverse callers and path
queries.

Reverse callers and between-symbol paths are opt-in:

```text
facts-tool analyse call-graph -f facts.db -c project.db \
  --function app::run --direction callers
facts-tool analyse call-graph -f facts.db -c project.db \
  --function app::start --to app::finish --path-mode all-simple
```

`callees` remains the default direction. Path mode defaults to `shortest`,
which returns one minimum-hop path using canonical USR and relation-site
tie-breaks. `all-simple` returns every deterministically ordered node-simple
path, so cycles cannot produce infinite results; a source equal to its target
is a valid zero-edge path. Exact qualified names and USRs are accepted.
Ambiguous names report every candidate identity and exit 2 so a USR can be
selected. `--direction callers` cannot be combined with `--to`, `--to` cannot
be combined with `--all`, and `--path-mode` requires `--to`.

`--max-depth N` adds a positive traversal cap. Without it, traversal continues
until a cycle, a reused context, or an external symbol. External boundaries
are complete stops and are not reported as truncation. Persisted edges keep
the stored relation kind, position, source location, and depth; cycle reuse
is recorded on the edge's `cycle` flag.

`-c/--conf` supplies the matching project catalog. When present, the command
validates the project/facts file identities and resolves source paths for
recovery. Without `--conf`, roots and recovery are limited to what the facts
store alone can resolve.

Version 8 extends `relation_site` in place with nullable `receiver_type_id`
and `certainty` columns. The migration does not rebuild the table or add a
context table, and it preserves the existing primary and foreign keys.
Version 12 adds the persisted call-graph run tables described below (see
[storage schema](storage-schema.md) for the full migration note).

## Persisted result contract (facts schema 11 -> 12)

`analyse call-graph` no longer prints a text listing, JSON document, or
Mermaid diagram. It traverses, then persists one append-only run per
invocation that reaches traversal, and prints exactly one completion line on
stdout:

```text
facts-tool: call graph run <run_id> <status>
```

`<status>` is one of `complete`, `truncated`, `cancelled`, `recovery-failed`,
or `failed`. The run is identified only by `<run_id>`; never identify an
invocation by scanning relation rows, since runs are never rewritten or
deleted by a later invocation.

### Run tables

| Table | Columns |
|---|---|
| `callgraph_run` | `run_id, created_at, project_path, facts_path, mode (callees\|callers\|path), path_mode (shortest\|all-simple\|NULL), calls_scope (all\|project\|library), components (comma-joined selected --component names, '' for all), max_depth, max_nodes, max_edges, time_limit_ms (NULL when not given), recover_missing (0/1), status, truncation_reason (NULL when none), error (NULL when none)` |
| `callgraph_run_root` | `run_id, symbol_id, usr` — selected roots (all definition roots for `--all`) |
| `callgraph_run_target` | `run_id, symbol_id, usr` — the `--to` target, when given |
| `callgraph_run_edge` | `run_id, source_id, destination_id, kind (1=Calls, 18=DispatchCalls), position, file_id, offset, depth, cycle` — only the `relation_site` rows actually reached |
| `callgraph_run_frontier` | `run_id, symbol_id, reason` |
| `callgraph_run_recovery` | `run_id, tu_file_id, outcome (attempted\|failed\|reused\|suppressed), diagnostic` |

Path mode persists only the edges on the found paths when at least one path
exists. When the target is unreachable the run still completes and keeps every
edge the search actually explored, so a non-empty edge set does not mean a
path was found. Check reachability by asking whether the target of
`callgraph_run_target` appears as a `destination_id` in the run's edges; a
source equal to its target is a found zero-edge path, whose run has a target
row and no edges. Callers mode persists reversed-direction reached sites
(`source_id` is the caller).

Intentionally **not** persisted: coverage/freshness/definition-availability
prose, external-boundary/definition-boundary labels, excluded-scope listings,
pair state, path results as a distinct object, `semantic_kind`, and
receiver/certainty on edges (join `relation_site` on
`source_id, destination_id, kind, position, file_id, offset` when a check
needs receiver, certainty, line, or column).

### Outcome matrix

| Outcome | stdout | stderr without `-v` | stderr with `-v` | exit | run row |
|---|---|---|---|---|---|
| complete | completion line | empty | verbose diagnostics only | 0 | yes (status `complete`) |
| truncated by budget (`--max-depth`/`--max-nodes`/`--max-edges`/`--time-limit-ms`) | completion line | empty | verbose diagnostics only | 0 | yes; `truncation_reason` = `max_depth\|max_nodes\|max_edges\|time_limit`; frontier rows carry that reason |
| usage error (unknown option, `--format`/`--output`, bad selector, `--to` with `--all`, invalid budget value, unknown `--component`) | empty | one `facts-tool: usage error: ...` line | same line plus verbose | 2 | no |
| configuration error (missing `--conf`, `--recover-missing` without a project configuration, `--component`/`--calls-scope` without a project/facts pair) | empty | one `facts-tool: configuration error: ...` line | same line plus verbose | 3 | no |
| database or operational error before traversal (missing facts db, no call facts for `--all`, invalid relation-site receiver context) | empty | one line | same line plus verbose | 1 | no |
| recovery failure (`--recover-missing`, a TU fails to compile) | completion line (status `recovery-failed`) | one `facts-tool: recovery failed for N translation unit(s); see callgraph_run_recovery run <id>` line | same line plus verbose | 1 | yes; `callgraph_run_recovery` has `outcome=failed` for that TU with the compiler diagnostic in `diagnostic`; the run keeps edges reached before the failure |
| operational failure after traversal started (status `failed`) | completion line | one line | same line plus verbose | 1 | yes; `error` column set |
| cancellation before traversal (SIGINT at the checkpoint after root selection) | empty | one `facts-tool: cancelled before traversal` line | same line plus verbose | 130 | no |
| cancellation during traversal or recovery | completion line (status `cancelled`) | one `facts-tool: cancelled; run <id> keeps the last usable generation` line | same line plus verbose | 130 | yes; `truncation_reason=cancelled`, a frontier row with reason `cancelled`, edges of the last usable generation |
| final commit failure (e.g. facts store read-only) | empty | one `facts-tool: cannot persist call graph run: <SQLite text>` line | same line plus verbose | 1 | no run and no child rows |

A `--conf` path that does not exist is reported the way every other command
reports it, as a database error (`project configuration database not found`,
exit 1), not as a configuration error.

Without `-v`, stderr never carries more than one line. With `-v`, that same
single error or summary line is present alongside verbose lines. stdout never
carries anything but the completion line or `--help` text.

At `-v 1` stderr adds `facts-tool: call-graph: starting`, `facts-tool: roots
selected`, `facts-tool: graph traversal`, recovery progress, compiler
diagnostics, and `facts-tool: call-graph: complete|failed`. `roots selected`
is emitted before the pre-traversal cancellation checkpoint, so a
cancellation there still shows which roots were selected. At `-v 2`, one
`facts-tool: root name='..' usr='..'` line follows per selected root. `-v 3`
adds trace detail including the recovery-validation line described in
[recovery](call-graph-recovery.md).

### Persistence rules

- Pre-traversal errors (usage, configuration, and most database/operational
  errors) write no run at all.
- A successful invocation writes its run in one transaction at the end;
  nothing is written incrementally during traversal.
- If that final commit fails (for example a read-only facts store), no run
  and no child rows are written; stderr reports
  `facts-tool: cannot persist call graph run: <SQLite text>` and the process
  exits 1.
- There are two cancellation checkpoints: one immediately after root
  selection (no run, exit 130) and one during traversal or recovery (a run
  with status `cancelled` covering the last usable generation, exit 130).

## Reading a run back

Graph results are never printed as text, JSON, or Mermaid; read the exact
persisted run through the installed public SDK:

```python
from facts_tool import open_codebase

with open_codebase(facts_db="facts.db", project_db="project.db") as cb:
    run_id = 1  # parse the id from native's completion line
    run = cb.callgraphs.get(run_id)
    print(run.run_id, run.status, run.path_outcome)
    for edge in run.edges:
        print(edge.source.qualified_name, edge.target.qualified_name)
    for item in run.recovery:
        print(item.outcome, item.diagnostic)
```

Use `target_reached`, `self_path`, and `path_found` for `--to` outcomes. Use
`truncated`, `truncation_reason`, `frontier`, and `recovery` to report limits
or recovery failures; a complete stored traversal does not prove complete
source extraction.
```

See the
[runnable skill recipe](../.agents/skills/facts-tool-code-reasoning/references/how-to-build-call-graph.md)
for a fixture-backed walkthrough, and
[symbol search](../.agents/skills/facts-tool-code-reasoning/references/how-to-search-symbol.md)
for exact selection and match-only index evidence.
