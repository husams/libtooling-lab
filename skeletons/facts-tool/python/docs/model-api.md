# Typed graph and fluent APIs

`CodeBase.executor` runs immutable plans; `CodeBase.graph` exposes
`GraphQuery`; `get(ref)`, `find(ref)`, and `query(ref=None)` delegate to that
shared implementation. `find` returns `None` only for a missing source and
retains ambiguity errors.

`GraphQuery` provides `get`, `find`, `query`, `neighbors`, `reaches`, `callers`,
`callees`, `bases`, `subclasses`, `members`, `parameters`, `definitions`, and
`references`. Relationship methods accept bounded depth and use stored graph
directions. Reference occurrences preserve site identity.

`Entity` exposes stored row fields as attributes plus `to_dict`, `outgoing`,
`incoming`, `definitions`, and `references`. `Callable` adds callers, callees,
and parameters. `Method` adds its record. `Record` adds bases, subclasses,
methods, and fields. Runtime subtype selection uses facts-tool's stored node
and index kind, not a guessed C++ spelling.

```python
with open_codebase(facts_db=facts, project_db=project) as cb:
    run = cb.get("app::run")
    print([callee.name for callee in run.callees(max_depth=3)])
    print(cb.query("app::run").relation("calls").names())
```

`EntityQuery` is lazy and immutable. Its `nodes`, `where`, `view`, `relation`,
`select`, `order_by`, and `limit` methods add portable stages; `plan` and
`to_plan()` expose the same `QueryPlan`. `run`, `all`, `names`, `count`, and
`first` execute it.

`filter(callable)` is explicitly local Python behavior. The callback is stored
outside the plan, runs after the shared executor, and never appears in
canonical JSON or a portable IR. Use predicate constructors for serializable
behavior.
