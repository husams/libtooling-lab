# Persisted call-graph runs

Schemas 12 through 14 store each native `analyse call-graph` result as an append-only
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

`CallGraphRun` also exposes `roots`, `targets`, `frontier`, `recovery`, `pointer_calls`,
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

Schema 14 records pointer invocations separately from traversable function edges:

```python
for call in run.pointer_calls:
    print(call.kind, call.source_id, call.target_name, call.signature,
          call.expression, call.file, call.line, call.column)
next_page = cb.callgraphs.get(run_id, limit=100, cursors={"pointer_calls": 100})
```

Each `CallGraphPointerCall` preserves the caller ID, optional pointer operand ID,
name and USR, canonical callable signature, callee expression, and source location.
The operand is a variable, parameter, or field; it is not a function edge or an
external function. Missing operand identity is represented by `None` while the
invocation remains recorded. The saved evidence survives later fact regeneration.
Schemas 12 and 13 return an empty `pointer_calls` page.

Schemas 10 and 11 retain the existing query API. Calling `cb.callgraphs` on
those stores raises `E_CAPABILITY`; malformed or future schema layouts raise
typed `FactsToolError` values while both stores remain unchanged.
