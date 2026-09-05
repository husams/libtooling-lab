# Language: stages and result shapes

`view(name)` selects a catalog view and `nodes()` enumerates it. `out` and
`in_` traverse a named relation over inclusive `min_depth..max_depth` windows.
Depth must be bounded, ordered, and at most 32. Nodes reached at any requested
depth are included once per logical domain, including a start node reached by
a cycle. Diamonds deduplicate their shared destination.

```python
prefix = start(symbol("app::run")) | out("calls", 1, 3)
query = prefix | where(glob("name", "app::*")) | select(("name",))
```

`union_`, `intersect`, and `except_` require compatible node views and use true
set semantics. `select(fields)` changes nodes to ordered rows. `distinct()`
deduplicates complete rows, `order_by(fields)` sorts nulls last with stable
logical-identity ties, `limit(n)` intentionally bounds output, and `count()`
produces a terminal scalar. Traversal cannot follow rows or scalars.

`sites()` changes matching edge nodes into the `site` view without losing
relation position or duplicate source locations. Selection and ordering after
`sites()` validate against site fields. A following `view("site")` is an
`E_STAGE` error because `sites()` already performed that transition.
`mode="devirtualized"` fails during validation and explain with `E_CAPABILITY`;
query the persisted `dispatch_calls` relation explicitly.

`path(to, relation, min_depth=1, max_depth=8, shortest=0, inbound=False)`
returns deterministic shortest simple witnesses per start. A repeat is
forbidden except a terminal cycle back to the start. Each stored-relation step
includes matching source sites. `rank(top_n=0)` orders by length and logical
identity. Only rank, distinct, limit, or count may follow a path.

`reverse_type_use(max_depth=8)` retains the cidx name but exposes facts-tool's
direct evidence only: `of_type`, return, parameter, and template-argument type
edges plus persisted parameter/template type fields. Results state the direct
`through` relation. Recursive normalized type-layer witnesses are unavailable.
Requests above direct depth return the available witnesses with `partial=True`.
