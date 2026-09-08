# Persisted call-graph runs

`analyse call-graph` is a separate, explicit native command that reads the
`calls`/`overrides`/`dispatch_calls` relations recorded by an ordinary
`extract` and performs a traversal. As of the schema-12 rewrite, it prints
exactly one line to stdout -

```text
facts-tool: call graph run <run_id> <status>
```

- and persists the full traversal (roots, targets, edges, frontier,
recovery attempts) as one **append-only run** across six
`callgraph_run*` tables. There is no text/JSON/Mermaid renderer anymore.
`CodeBase.callgraphs` (a `CallGraphReader`) is the read-only SDK surface
over those tables.

This capability - the schema-12 callgraph-run reader described in this
entire chapter - **shipped to main in PR #76** (Backlog story S-029). It is
gated to facts schema **12 only**; opening a schema-10 or schema-11
database and calling any `cb.callgraphs.*` method fails `E_CAPABILITY`.
Nothing in this chapter recomputes, replays, or re-derives a traversal -
every method here only reads rows a prior `analyse call-graph` invocation
already committed. Ordinary `cb.graph.callees()`/`callers()` (see
[05-relations-and-graph-queries.md](05-relations-and-graph-queries.md)) are
live relation navigation, computed fresh on every call, and are never
mislabeled as a graph run.

## The CLI commands that produced the run data in this chapter

Three call-graph runs were persisted into the same facts database used
throughout this chapter's examples:

```console
$ facts-tool analyse call-graph -v 0 -f facts.sqlite -c project.sqlite --function app::run
facts-tool: call graph run 1 complete

$ facts-tool analyse call-graph -v 0 -f facts.sqlite -c project.sqlite --function app::run --to app::persist
facts-tool: call graph run 2 complete

$ facts-tool analyse call-graph -v 0 -f facts.sqlite -c project.sqlite --function app::run --to app::dispatch_probe
facts-tool: call graph run 3 complete
```

Run 1 has no `--to` (an open traversal from the root). Run 2 asks whether
`app::persist` is reachable from `app::run` (it is). Run 3 asks the same
of `app::dispatch_probe` (it is not - nothing in this fixture calls it).
See [01-overview.md](../04-call-graphs/01-overview.md) and
[02-generating-a-call-graph.md](../04-call-graphs/02-generating-a-call-graph.md)
for the full native CLI surface.

## `CallGraphReader`

```python
class CallGraphReader:
    def list(self, *, limit: int = 100, after: int | None = None) -> tuple[CallGraphRun, ...]: ...
    def latest(self) -> CallGraphRun | None: ...
    def get(self, run_id: int, *, limit: int = 1000, offset: int = 0,
            cursors: Mapping[str, int] | None = None) -> CallGraphRun: ...
```

`limit` must be a positive `int` (not `bool`); `after`/`offset`/each
`cursors` value must be a non-negative `int` (not `bool`); an unknown
cursor collection key fails `E_LIMIT`; a nonexistent `run_id` fails
`E_SOURCE`. All three methods first check the facts schema is exactly 12:

```python
with open_codebase(facts_db="facts.sqlite", project_db="project.sqlite") as cb:
    print([r.run_id for r in cb.callgraphs.list(limit=10)])
```

```text
[1, 2, 3]
```

Against a schema-10 database:

```python
cb.callgraphs.latest()
```

```text
E_CAPABILITY: persisted call graph runs require facts schema 12
```

### `list()` gives child collections at most one item each

`list()` calls `get(run_id, limit=1)` internally for every run header it
returns - every run's `roots`, `targets`, `edges`, `frontier`, and
`recovery` pages in a `list()` result are truncated to at most one item
each, even if the run actually contains more. Verified:

```python
for r in cb.callgraphs.list(limit=10):
    print(r.run_id, r.status, r.edges.total, len(r.edges.items), r.edges.truncated)
```

```text
1 complete 3 1 True
2 complete 2 1 True
3 complete 3 1 True
```

`edges.total` (3) correctly reports the real count; `len(edges.items)` (1)
does not. Use `.latest()` or `.get(run_id)` - not `.list()` - whenever you
need a run's actual child rows, not just its header and totals.

```python
latest = cb.callgraphs.latest()
print(latest.run_id, latest.edges.total, len(latest.edges.items))
```

```text
3 3 3
```

## `CallGraphRun` fields

```python
@dataclass
class CallGraphRun:
    run_id: int
    created_at: str
    project_path: str
    facts_path: str
    mode: str
    path_mode: str | None
    calls_scope: str
    components: tuple[str, ...]
    max_depth: int | None
    max_nodes: int | None
    max_edges: int | None
    time_limit_ms: int | None
    recover_missing: bool
    status: str
    truncation_reason: str | None
    error: str | None
    roots: CallGraphPage[CallGraphRoot]
    targets: CallGraphPage[CallGraphTarget]
    edges: CallGraphPage[CallGraphEdge]
    frontier: CallGraphPage[CallGraphFrontier]
    recovery: CallGraphPage[CallGraphRecovery]
    provenance: PairProvenance
    target_exists: bool
    target_was_reached: bool
    self_path_exists: bool
```

The last three fields are the decoded run flags the outcome rules below are
computed from. Derived properties: `.target` (first target, or `None`),
`.sites` (flattened non-`None` edge sites), `.target_reached`,
`.self_path`, `.path_found`, `.path_outcome`, `.truncated` (true if any
child page truncated), `.boundaries` (alias for `.frontier`).

```python
r1 = cb.callgraphs.get(1)
print(r1.status, r1.path_outcome, r1.roots.total, r1.edges.total)
for e in r1.edges:
    print(e.source.qualified_name, "->", e.target.qualified_name, e.semantic_kind, e.depth, e.cycle)
```

```text
complete not-applicable 1 3
app::run -> app::save Calls 1 False
app::run -> app::save Calls 1 False
app::save -> app::persist Calls 2 False
```

Run 1 has no `--to`, so `path_outcome == "not-applicable"` even though
`status == "complete"` - there is simply no target to have found or missed.

## Roots, targets, and `path_outcome`

```python
r2 = cb.callgraphs.get(2)
r3 = cb.callgraphs.get(3)
print(r2.target.symbol.qualified_name, r2.target_reached, r2.path_outcome, r2.path_found)
print(r3.target.symbol.qualified_name, r3.target_reached, r3.path_outcome, r3.path_found)
```

```text
app::persist True found True
app::dispatch_probe False unreachable False
```

`CallGraphRun.path_outcome` logic, verified against the parametrized test
matrix and the three real runs above:

1. No target recorded on the run -> `"not-applicable"`.
2. The target was reached, or a self-path exists (root == target) ->
   `"found"`.
3. `status == "complete"` and neither of the above -> `"unreachable"`.
4. `status` in `{truncated, cancelled, recovery-failed, failed}` -> that
   same string is surfaced as the outcome directly.
5. Otherwise -> `"unknown"`.

There are five `status` values: `complete`, `truncated`, `cancelled`,
`recovery-failed`, and `failed`. `complete` and `truncated` were both
reproduced live for this chapter; the remaining three are documented
native-writer states your code should still handle.

Three more runs against the same database reproduce `truncated` and give the
frontier and recovery sections below something to read:

```console
$ facts-tool analyse call-graph -v 0 -f facts.sqlite -c project.sqlite --function app::run --max-depth 1
facts-tool: call graph run 4 truncated

$ facts-tool analyse call-graph -v 0 -f facts.sqlite -c project.sqlite --function app::run --max-nodes 2
facts-tool: call graph run 5 truncated

$ facts-tool analyse call-graph -v 0 -f facts.sqlite -c project.sqlite --function app::run --to app::persist --max-depth 1
facts-tool: call graph run 6 truncated
```

Run 6 shows rule 4 in action: it has a target, did not reach it, and is not
`complete`, so the status is surfaced as the outcome rather than
`unreachable`. Runs 4 and 5 show rule 1 winning over rule 4: they are
equally truncated, but with no `--to` there was never a target to reach.

```python
for run_id in (4, 5, 6):
    run = cb.callgraphs.get(run_id)
    print(run.run_id, run.status, run.truncation_reason, run.path_outcome, run.target_reached)
```

```text
4 truncated max_depth not-applicable False
5 truncated max_nodes not-applicable False
6 truncated max_depth truncated False
```

## Edges: semantic kind, depth, cycles, sites

```python
@dataclass
class CallGraphEdge:
    source: CallGraphSymbol
    target: CallGraphSymbol
    kind_id: int
    kind: str
    semantic_kind: str
    position: int
    file_id: int
    file: str | None
    line: int | None
    column: int | None
    offset: int
    depth: int
    cycle: bool
    site: CallGraphSite | None
```

`kind_id` is hard-restricted to `{1: "calls", 18: "dispatch_calls"}` - any
other persisted relation kind in a `callgraph_run_edge` row would fail
`E_SCHEMA` when decoded. A call-graph run can therefore only ever surface
plain calls and virtual-dispatch calls, never (for example) `uses` or
`construct_value` edges, even if a future native writer started persisting
those under the same tables. `semantic_kind` is a title-cased,
underscore-stripped rendering of `kind`: `"calls"` becomes `"Calls"`,
`"dispatch_calls"` becomes `"DispatchCalls"` - verified above as
`"Calls"`.

`.sites` on `CallGraphRun` flattens every non-`None` edge site across the
whole run. `CallGraphEdge.site` (singular) is at most one
`CallGraphSite` per persisted edge row - a `CallGraphEdge` never carries
more than one site, even when the same call appears at multiple lexical
positions; those fold into the parent relation row's own location instead
of multiplying out here.

```python
@dataclass
class CallGraphSite:
    file_id: int
    file: str | None
    line: int | None
    column: int | None
    offset: int
    receiver_type_id: int | None
    certainty: int | None
    enriched: bool = False
```

`enriched=True` only when a matching `relation_site` evidence row was
actually joined to the persisted edge; an edge without matching site
evidence gets a bare `CallGraphSite` with `enriched=False` and
`line=column=receiver_type_id=certainty=None`.

## Paging one collection independently

```python
cursor = 0
while True:
    page = cb.callgraphs.get(1, limit=1, cursors={"edges": cursor})
    if not page.edges.items:
        break
    e = page.edges.items[0]
    print(cursor, e.source.qualified_name, e.target.qualified_name, page.edges.next_cursor)
    if page.edges.next_cursor is None:
        break
    cursor = page.edges.next_cursor
```

```text
0 app::run app::save 1
1 app::run app::save 2
2 app::save app::persist None
```

`CallGraphPage[T]` exposes `.items`, `.total`, `.next_cursor`,
`.complete`/`.truncated` properties, and `__iter__`/`__len__`/`__getitem__`.
`get(run_id, limit=1, cursors={"edges": N})` pages the `edges` collection
independently of `roots`/`targets`/`frontier`/`recovery`, which otherwise
all default to the same `offset`/cursor.

## Frontier / boundaries and recovery diagnostics

```python
@dataclass
class CallGraphFrontier:
    symbol: CallGraphSymbol
    reason: str

@dataclass
class CallGraphRecovery:
    tu_file_id: int
    file: str | None
    outcome: str
    diagnostic: str | None
```

A `CallGraphFrontier` row is a discovered-but-not-admitted graph endpoint
recorded when a budget or time limit stops traversal. `reason` is a raw
native-writer string; the values the traversal emits are `max_depth`,
`max_nodes`, `max_edges`, `time_limit`, and `cancelled`. Runs 1 to 3
produced no frontier rows, because none of the `--max-*` options were used.
Runs 4 and 5 from the previous section do:

```python
for run_id in (4, 5):
    run = cb.callgraphs.get(run_id)
    print(run.run_id, run.status, run.truncation_reason,
          [(f.symbol.qualified_name, f.reason) for f in run.frontier])
```

```text
4 truncated max_depth [('app::save', 'max_depth')]
5 truncated max_nodes [('app::persist', 'max_nodes')]
```

`CallGraphRecovery` records one row per translation unit the native writer
had to recover graph evidence for under `--recover-missing`. `outcome` is
constrained by the storage schema itself to `attempted`, `failed`, `reused`,
or `suppressed`, so those four are the complete vocabulary rather than a
sample. A `--recover-missing` run over this already-indexed fixture records
`reused`:

```console
$ facts-tool analyse call-graph -v 0 -f facts.sqlite -c project.sqlite --function app::run --recover-missing
facts-tool: call graph run 7 complete
```

```python
run = cb.callgraphs.get(7)
print([(r.tu_file_id, r.outcome, r.diagnostic) for r in run.recovery])
```

```text
[(1, 'reused', 'existing valid body and calls')]
```

`failed` and `suppressed` were not reproduced here; they need a translation
unit whose evidence recovery actually fails, which this single fully-indexed
fixture cannot produce. See
[04-recovery-and-boundaries.md](../04-call-graphs/04-recovery-and-boundaries.md)
for the native-side semantics these fields surface.

## Locationless symbols

```python
@dataclass
class CallGraphSymbol:
    symbol_id: int
    usr: str
    qualified_name: str
    file_id: int
    file: str | None
    line: int | None
    column: int | None
```

`file`/`line`/`column` are all `None` whenever the symbol's `FileId` can't
be resolved to a path - compiler-provided `FileId 0` (see
[02-what-gets-extracted.md](../03-extracting-facts/02-what-gets-extracted.md)
for locationless callables), or a symbol whose declaring file simply isn't
present in the paired project database. The reader never invents a path
for these; a `None` here is a real "no location known" fact, not a bug.

## Relation-kind mapping, restated

A call-graph run's edges cover exactly two of the 23 stored relations:
`calls` (kind id 1) and `dispatch_calls` (kind id 18). Every other stored
relation - `inherits`, `uses`, `field_of`, `construct_*`, `of_type`, and so
on - is available through ordinary relation navigation (see
[05-relations-and-graph-queries.md](05-relations-and-graph-queries.md)) but
never through a persisted call-graph run.

Continue to [07-error-handling.md](07-error-handling.md) for the complete
error code reference, including every code introduced in this chapter.
