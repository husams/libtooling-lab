# Call Graphs: Overview

`analyse call-graph` is a separate, explicitly requested command that reads
the `Calls`/`Overrides`/`DispatchCalls` relations `extract` already recorded
and traverses them. Ordinary `extract` never builds or maintains any
separate graph-analysis structure on its own - this was a deliberate design
correction: extraction records call evidence once, during the normal
extraction pass; graph traversal is a distinct, opt-in operation you run
against that evidence afterward.

## Native, SQLite-only - no text, JSON, or diagram output

`analyse call-graph` traverses, then persists the entire result as one
**append-only run** in six `callgraph_run*` tables in the facts database,
and prints exactly one completion line to stdout:

```text
facts-tool: call graph run <run_id> <status>
```

Nothing else is printed on success - no node/edge listing, no JSON
document, no Mermaid diagram. This is current, shipped behavior: an earlier
version of the tool did render text/JSON/Mermaid output directly, but that
renderer was deliberately removed. If you find older material describing
`--format`/`--output` flags or inline diagrams for `analyse call-graph`
itself, it describes a version of the tool that predates this rewrite and
no longer applies. Read a run back through SQLite directly, or (preferred,
for anything beyond ad hoc inspection) the Python SDK's persisted-run reader
- see [Entries, Runs, and Status](03-entries-runs-and-status.md) and
[the SDK's persisted call-graph runs chapter](../05-python-sdk/06-persisted-callgraph-runs.md).

`analyse call-graph-entry` is the one exception: it prints a single
function's metadata as `text` or `json`, but that is a per-symbol record,
not the graph itself - see
[Generating a Call Graph](02-generating-a-call-graph.md).

## Roots

Select the traversal's starting point one of two ways:

- `--function SELECTOR` - an exact qualified name or USR. The match is
  exact: a selector that is neither a stored qualified name nor a stored USR
  fails with `usage error: missing-root: selector '<name>' was not found`
  (exit 2). A name that several symbols share (overloads) fails with
  `usage error: ambiguous-root: selector '<name>' matches [...]; select a
  USR` (exit 2), listing the candidates so you can pick one.
- `--all` - every definition-backed symbol that has calls, each treated as
  its own root.

## Direction: callees vs. callers

`callees` (forward, the default) traces what a root calls, transitively.
`--direction callers` reverses this to trace what calls into the root:

```console
$ facts-tool analyse call-graph -c demo.db -f demo-facts.db --function main -v 1
facts-tool: call-graph: starting
facts-tool: roots selected
facts-tool: graph traversal
facts-tool: call-graph: complete
facts-tool: call graph run 1 complete
```

```console
$ facts-tool analyse call-graph -c demo.db -f demo-facts.db --function 'shapes::Circle::area' --direction callers
facts-tool: call graph run 2 complete
```

In callers mode, the persisted edges still use `source_id`/`destination_id`
in the same columns, but `source_id` is now the caller - the direction flag
changes which end of each edge is "reached from" the root, not the schema.

## Budgets and depth

Traversal has **no application-level cap by default**: it continues across
every registered component and available library edge until a cycle, a
reused context, or an external symbol provides a semantic stop. This is a
deliberate choice - call-graph reasoning is meant to span every registered
repository/component by default, not silently restrict itself to whatever
checkout you happened to run the command from.

Optional, request-only budgets narrow this:

| Flag | Limits |
|---|---|
| `--max-depth N` | call-edge hops from the root (root counts as depth 0/one node) |
| `--max-nodes N` | canonical symbol identities admitted |
| `--max-edges N` | canonical relation keys admitted |
| `--time-limit-ms N` | wall-clock budget, monotonic clock |

None of these are on by default; every stop they cause is recorded as a
**truncation**, not silently dropped.

## Truncation

A budget stop sets `truncation_reason` on the run row (`max_depth`,
`max_nodes`, `max_edges`, or `time_limit`) and adds one or more
`callgraph_run_frontier` rows recording exactly which discovered-but-not-
admitted nodes were cut. Real example, `--max-depth 1` against the same
demo project:

```console
$ facts-tool analyse call-graph -c demo.db -f demo-facts.db --function main --max-depth 1
facts-tool: call graph run 3 truncated
```

The run row records `status='truncated', truncation_reason='max_depth'`, and
the frontier holds the node that was admitted at depth 1 but whose expansion
the cap prevented - `(anonymous namespace)::totalArea` in this run, with
`reason='max_depth'`. The depth-1 endpoints with no further project-side
callees (the `std::` targets) are external boundaries, not frontier rows.
**A `truncated` run still exits 0** - truncation is a
successful-but-incomplete outcome, not an error.

## Coverage vs. completeness

These are two genuinely separate questions, and the guide is careful not to
conflate them:

- **Traversal completeness** - did this specific `analyse call-graph`
  invocation exhaust the stored graph reachable from its roots, under its
  given scope and budget? This is what `status` answers.
- **Extraction coverage** - has a given function's call evidence actually
  been committed by a prior `extract`? This is what a
  [function entry](03-entries-runs-and-status.md#function-entries) answers,
  and it is a completely separate axis: a run can be `complete` while some
  reachable function was never fully extracted, or while a definition is
  genuinely external to the project.

A third axis, freshness - whether stored facts still match the file on disk
right now - is separate again; see
[Recovery and Boundaries](04-recovery-and-boundaries.md).

## Virtual dispatch: `DispatchCalls`

A virtual call is persisted as `dispatch_calls` (kind 18), deliberately
**conservative**: it may include targets that are not actually reachable at
runtime for a given static type, rather than under-approximating. A real
traversal through a virtual `area()` call inside `describe(const Shape&)`
correctly resolved to both concrete overrides:

```text
('shapes::describe', 'shapes::Circle::area', 18, 3),
('shapes::describe', 'shapes::Square::area', 18, 3)
```

## External boundaries

A call target whose declaration is known but whose definition is not in the
project (or has not yet been extracted) is a **complete stop, not a
truncation**. It is reported as `status='complete'` with no frontier row -
the traversal genuinely finished at that node, it just did not have a body
to descend into. This is distinct from a budget stop, and distinct from an
indirect/unresolved call, which has no destination row at all and is
recorded separately. See
[Recovery and Boundaries](04-recovery-and-boundaries.md) for exactly how
boundaries are recorded and how `--recover-missing` can resolve some of
them.
