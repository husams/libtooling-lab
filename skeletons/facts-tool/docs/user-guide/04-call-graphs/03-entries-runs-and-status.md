# Entries, Runs, and Status

A call graph in facts-tool has two independent kinds of persisted record:
**function entries**, which say whether a symbol's call evidence has ever
been fully collected, and **runs**, which say what one specific
`analyse call-graph` invocation found. This chapter defines both and how
project mutations invalidate them.

## Runs: identity and status

Every successful `analyse call-graph` invocation that reaches traversal
writes exactly one row to `callgraph_run`, identified only by its
`run_id`. Runs are **append-only** - never rewritten, never deleted by a
later invocation. Never identify a specific invocation's results by
scanning `relation`/`relation_site` rows; scan the run tables instead.

`callgraph_run` columns: `run_id, created_at, project_path, facts_path,
mode (callees|callers|path), path_mode (shortest|all-simple|NULL),
calls_scope (all|project|library), components (comma-joined selected
--component names, '' for all), max_depth, max_nodes, max_edges,
time_limit_ms (NULL when not given), recover_missing (0/1), status,
truncation_reason (NULL when none), error (NULL when none)`.

`status` is one of:

| Status | Meaning |
|---|---|
| `complete` | traversal exhausted the reachable graph under the given scope/budget |
| `truncated` | a budget (`max_depth`/`max_nodes`/`max_edges`/`time_limit`) stopped traversal early; exit is still 0 |
| `cancelled` | SIGINT arrived during traversal or recovery; the run keeps "the last usable generation"; exit 130 |
| `recovery-failed` | `--recover-missing` was given and a translation unit failed to recover; exit 1; edges reached before the failing TU are kept |
| `failed` | an operational failure occurred after traversal started; the `error` column is set; exit 1 |

A `--conf` path that does not exist is reported as a database error
(exit 1), never as a configuration error, the same as every other command.
Pre-traversal errors (usage, configuration, most database/operational
errors) write **no run row at all**; a SIGINT that arrives before traversal
starts (at the checkpoint right after root selection) also writes no run
and exits 130.

### The five child tables

| Table | Holds |
|---|---|
| `callgraph_run_root` | selected roots (`run_id, symbol_id, usr`) - all definition roots for `--all` |
| `callgraph_run_target` | the `--to` target, when given (`run_id, symbol_id, usr`) |
| `callgraph_run_edge` | only the `relation_site` rows actually reached (`run_id, source_id, destination_id, kind (1=Calls, 18=DispatchCalls), position, file_id, offset, depth, cycle`) |
| `callgraph_run_frontier` | discovered-but-not-admitted endpoints (`run_id, symbol_id, reason`) |
| `callgraph_run_recovery` | one row per attempted translation unit during `--recover-missing` (`run_id, tu_file_id, outcome, diagnostic`) |

Intentionally **not** persisted on a run: coverage/freshness/definition-
availability prose, external-boundary labels, excluded-scope listings, pair
state, a distinct "path result" object, `semantic_kind`, or receiver/
certainty on edges. To recover receiver type and certainty for a specific
edge, join `relation_site` on
`(source_id, destination_id, kind, position, file_id, offset)`.

### Determining reachability for a `--to` query

A non-empty edge set does not by itself mean a path was found - when the
target is unreachable, the run still completes and keeps every edge it
explored along the way. Reachability is: the target's symbol ID appears as
a `destination_id` among the run's edges, **or** the target equals a root
(a valid zero-edge self-path). Use a public reader to check this rather
than re-deriving it by hand - see below.

## Function entries

A **function entry** is a separate concept from a run: it records whether a
specific function's call evidence has been fully, successfully collected at
least once, independent of which (if any) `analyse call-graph` run reached
it. All entries refer to the shared symbol/relation graph - asking about
two different roots never stores two copies of their shared callees.

| Stored evidence | `entry_available` | `graph_node_ref` | `is_leaf` |
|---|---|---|---|
| Fully collected body with outgoing calls | `true` | symbol ID | `false` |
| Fully collected body, no outgoing/unresolved calls | `true` | symbol ID | `true` |
| Known symbol, no committed generation | `false` | `null` | `null` |

Entries are only published by `extract` - never by `analyse call-graph` or
`match`. A narrow symbol or call match never certifies that the caller's
entire body was collected; only a subsequent full extraction republishes a
completed entry, including leaves. Entry availability is independent of
freshness and of transitive graph completeness (see
[Overview](01-overview.md#coverage-vs-completeness)); a `complete` run and a
`true` entry answer two different questions and neither implies the other.

A known declaration-only call target (no definition in the project yet)
keeps its canonical symbol identity and exact call site in a
`callgraph_external_reference` record. Extracting its real definition later
(from another registered component) reuses that identity, retains its
callers and sites, and removes the resolved external boundary - see
[Recovery and Boundaries](04-recovery-and-boundaries.md).

## How mutations invalidate entries

Most project-configuration mutations - a changed compile command via
`import`, or a catalog edit made with `--facts` on `repo`/`component`/
`dir`/`file` - conservatively invalidate **every entry** in the selected
facts store; only a subsequent full extraction republishes them. A
library-only extraction, or an extraction whose sources produce zero
matching evidence, also clears caller entries; extract every source file
you want represented together to republish them all. Listing and dry-run
catalog commands never invalidate anything. See
[Extracting Facts](../03-extracting-facts/01-extract.md#how-catalog-mutations-invalidate-call-graph-entries)
for the exact `--facts` mechanics.

## Reading runs from Python

Runs are schema-12-only. The Python SDK's `CodeBase.callgraphs` reader is
the supported, paged, typed way to read them back - full coverage,
including paging and every field, lives in
[Persisted Call-Graph Runs](../05-python-sdk/06-persisted-callgraph-runs.md).
A short taste of run discovery, against the sample project the SDK chapters
use (`facts.sqlite` plus `project.sqlite`) after three runs had been
recorded in it:

```console
$ python -c "
from facts_tool import open_codebase
with open_codebase(facts_db='facts.sqlite', project_db='project.sqlite') as cb:
    print([r.run_id for r in cb.callgraphs.list(limit=10)])
    r1 = cb.callgraphs.get(1)
    print(r1.status, r1.path_outcome, r1.roots.total, r1.edges.total)
"
[1, 2, 3]
complete not-applicable 1 3
```

`r1.path_outcome == 'not-applicable'` here because run 1 had no `--to`
target, even though `r1.status == 'complete'`.
