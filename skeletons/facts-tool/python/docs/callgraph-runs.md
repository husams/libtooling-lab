# Persisted call-graph runs

Schemas 12 and 13 store each native `analyse call-graph` result as an append-only
run. Capture the completion `run_id`, open the same paired database read-only,
then use `cb.callgraphs.get(run_id)`; this API only reads the persisted run and
never replays traversal.

```python
from facts_tool import open_codebase

with open_codebase(facts_db="facts.sqlite", project_db="project.sqlite") as cb:
    run_id = 1  # parse the id from native: call graph run <id> <status>
    run = cb.callgraphs.get(run_id)
    print(run.run_id, run.status, run.path_found)
    for edge in run.edges:
        print(edge.source.qualified_name, edge.semantic_kind,
              edge.target.qualified_name, edge.site)
```

`CallGraphRun` also exposes `roots`, `targets`, `frontier`, `recovery`,
`truncation_reason`, limits, `error`, and pair `provenance`. A positive `limit`
keeps every child collection bounded; each collection exposes `total`,
`complete`, and `next_cursor`, and `get(run_id, limit=..., cursors={...})`
reads the next bounded page independently. `list(limit=..., after=...)` pages
runs without hydrating unbounded child collections.
`target_reached`, `self_path`, and `path_found` distinguish an unreachable
target from a zero-edge self path. `edges` are exact persisted graph evidence;
ordinary `cb.graph.callees()` and `cb.graph.callers()` remain independent
relation navigation and are never relabelled as a graph run.
`path_outcome` reports `found`, `unreachable`, `not-applicable`, or the native
status for `truncated`, `cancelled`, `recovery-failed`, and `failed` runs.

Schemas 10 and 11 retain the existing query API. Calling `cb.callgraphs` on
those stores raises `E_CAPABILITY`; malformed or future schema layouts raise
typed `FactsToolError` values while both stores remain unchanged.
