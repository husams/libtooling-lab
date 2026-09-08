# Relations and graph queries

`CodeBase.graph` is a `GraphQuery` - a typed facade over the same
`Executor`/`Plan` machinery from
[03-query-model.md](03-query-model.md), returning `Entity` objects (and
subtypes `Callable`, `Method`, `Record`) instead of raw rows wherever a
query result is itself a node. `CodeBase.get`/`.find`/`.query` delegate
straight to `cb.graph.get`/`.find`/`.query`.

## `Entity`, `Callable`, `Method`, `Record`

```python
class Entity:
    def __getattr__(self, name: str) -> Any: ...      # proxies into the underlying row
    def to_dict(self) -> Row: ...                       # strips "_"-prefixed internal keys
    def outgoing(self, relation: str, max_depth: int = 1) -> list["Entity"]: ...
    def incoming(self, relation: str, max_depth: int = 1) -> list["Entity"]: ...
    def definitions(self) -> list[Row]: ...
    def references(self) -> list[Row]: ...

class Callable(Entity):
    def callers(self, max_depth: int = 1) -> list[Entity]: ...
    def callees(self, max_depth: int = 1) -> list[Entity]: ...
    def parameters(self) -> list[Row]: ...

class Method(Callable):
    def record(self) -> Entity | None: ...

class Record(Entity):
    def bases(self, max_depth: int = 1) -> list[Entity]: ...
    def subclasses(self, max_depth: int = 1) -> list[Entity]: ...
    def methods(self) -> list[Entity]: ...
    def fields(self) -> list[Entity]: ...
```

`make_entity` picks the concrete subtype by inspecting the row's stored
`node_kind`/`kind` directly - never a guessed C++ spelling. `Entity`
attribute access (`entity.name`, `entity.qualified_name`, ...) proxies
straight into the underlying row via `__getattr__`.

```python
with open_codebase(facts_db="facts.sqlite", project_db="project.sqlite") as cb:
    run = cb.get("app::run")
    print(type(run).__name__)
    print([callee.qualified_name for callee in run.callees(max_depth=3)])
    print(cb.query("app::run").relation("calls").names())
```

```text
Callable
['app::save', 'app::persist']
['save']
```

`callees(max_depth=3)` reaches transitively through `save` -> `persist`; the
fluent one-hop `.relation("calls")` stops at `save`. Both are correct for
their requested depth - `run` calls `save` twice (deduplicated to one
node) and `save` calls `persist` once.

A `Record` example, walking `app::Box`, which publicly inherits
`app::Base` and overrides its pure virtual `flush`:

```python
box = cb.get("app::Box")
print(type(box).__name__)
print([m.qualified_name for m in box.methods()])
print([f.qualified_name for f in box.fields()])
print([b.qualified_name for b in box.bases()])
```

```text
Record
['app::Box::flush']
['app::Box::value']
['app::Base']
```

`Method.record()` walks back to the owning record:

```python
flush = cb.get("app::Box::flush")
print(type(flush).__name__)
print(flush.record().qualified_name)
```

```text
Method
app::Box
```

## `GraphQuery`

```python
class GraphQuery:
    def get(self, ref: str) -> Entity: ...
    def find(self, ref: str) -> Entity | None: ...           # swallows only "not found" E_SOURCE
    def query(self, ref: str | None = None) -> "EntityQuery": ...
    def neighbors(self, ref, relation, *, inbound=False, min_depth=1, max_depth=1) -> list[Entity]: ...
    def reaches(self, source, target, relation, max_depth=8) -> bool: ...
    def callers(self, ref: str, max_depth: int = 1) -> list[Entity]: ...
    def callees(self, ref: str, max_depth: int = 1) -> list[Entity]: ...
    def bases(self, ref: str, max_depth: int = 1) -> list[Entity]: ...
    def subclasses(self, ref: str, max_depth: int = 1) -> list[Entity]: ...
    def members(self, ref: str) -> list[Entity]: ...
    def parameters(self, ref: str) -> list[Row]: ...
    def definitions(self, ref: str) -> list[Row]: ...
    def references(self, ref: str) -> list[Row]: ...
```

`.find(ref)` returns `None` only when the underlying error is `E_SOURCE`
with `"not found"` in its message; every other `FactsToolError` still
propagates.

```python
print(cb.find("app::does_not_exist"))
```

```text
None
```

### `neighbors` and directionality

`neighbors` is the shared primitive behind `callers`/`callees`/`bases`/
`subclasses`; it traverses `out(relation, ...)` unless `inbound=True`, in
which case it traverses `in_(relation, ...)`:

```python
print([e.qualified_name for e in cb.graph.neighbors("app::save", "calls", inbound=True)])
print([e.qualified_name for e in cb.graph.neighbors("app::save", "calls", inbound=False)])
```

```text
['app::run']
['app::persist']
```

`callers` is `neighbors(ref, "calls", inbound=True, ...)`; `callees` is
`neighbors(ref, "calls", ...)`. `bases` is `neighbors(ref, "inherits", ...)`;
`subclasses` is `neighbors(ref, "inherits", inbound=True, ...)`.

```python
print([e.qualified_name for e in cb.graph.bases("app::Box")])
print([e.qualified_name for e in cb.graph.subclasses("app::Base")])
```

```text
['app::Base']
['app::Box', 'app::Box']
```

(Two `app::Box` rows again reflect the template pattern and its explicit
instantiation, both persisted as distinct symbols that inherit `app::Base`.)

### `reaches`

```python
print(cb.graph.reaches("app::run", "app::persist", "calls", max_depth=3))
print(cb.graph.reaches("app::run", "app::dispatch_probe", "calls", max_depth=3))
```

```text
True
False
```

`reaches` checks whether any neighbor's `usr` or `qualified_name` equals
`target` after traversing up to `max_depth` - it is a thin convenience over
`neighbors`, not a separate reachability algorithm, so its budget/cost
characteristics are identical to a direct `out`/`in_` traversal.

### `members`, `parameters`, `definitions`

`members(ref)` traverses the `contains` relation (lexical scope to
declaration) - note this is a different, coarser relation than `fields()`/
`methods()` on a `Record`, which use `field_of`/`method_of` instead:

```python
print(cb.graph.members("app::Box"))
```

```text
[]
```

(`contains` does not include fields/methods of a struct in this fixture;
use `Record.fields()`/`.methods()`, or `out("has_field")`... which does not
exist as a relation - use the inbound `field_of`/`method_of` relations, or
the `has_field`/`has_method` **predicates** from
[03-query-model.md](03-query-model.md), when you need field/method
membership.)

`parameters(ref)` reads the `has_parameter` pseudo-relation and returns raw
rows, not typed entities:

```python
print([(p["name"], p["has_default"]) for p in cb.graph.parameters("app::run")])
```

```text
[('box', True)]
```

`definitions(ref)` reads the `definition` pseudo-relation, returning rows
with `file_id`/`offset`/`size` (no line/column - see
[04-views-and-catalog.md](04-views-and-catalog.md)):

```python
print([dict(d) for d in cb.graph.definitions("app::run")])
```

```text
[{'symbol_id': 8589934612, 'file_id': 1, 'offset': 182, 'size': 75, 'id': 'definition:8589934612', 'file': '.../source.cpp', '_key': 'definition:8589934612', '_view': 'definition'}]
```

### `references` - a scaling caveat

`references(ref)` loads the **entire** `edge` -> `sites()` view and filters
client-side by `destination_id == get(ref).id`. This is O(all relation
sites in the database), not indexed - fine for a demo database, but a real
cost to be aware of before calling it inside a loop over many symbols in a
large codebase:

The rows are `site` rows, so the column field is spelled `col`, not
`column`:

```python
refs = cb.graph.references("app::save")
print(len(refs), [(r["line"], r["col"]) for r in refs][:3])
```

```text
2 [(13, 10), (13, 19)]
```

`app::run` calls `app::save` twice on the same source line, and each call
site is its own row. `Entity.references()` on `cb.get("app::save")` returns
the identical rows.

## Paths and witnesses

`path(to, relation, min_depth=1, max_depth=8, shortest=0, inbound=False)`
returns deterministic shortest **simple** witnesses per start - a repeat is
forbidden except a terminal cycle back to the start. `rank(top_n=0)`
orders witnesses by `(length, logical id)`; it is the only stage allowed
after `path` besides `distinct`/`limit`/`count`.

```python
from facts_tool.queryplan import start, symbol, path, rank

witness = (
    start(symbol("app::run"))
    | path(start(symbol("app::persist")), "calls")
    | rank(5)
)
result = cb.executor.run(witness.plan)

found = result.paths[0]
print(result.shape, result.truncated, found["length"])
print(found["start"]["qualified_name"], "->", found["end"]["qualified_name"])
for step in found["steps"]:
    sites = [(s["file"].rsplit("/", 1)[-1], s["line"], s["col"]) for s in step["sites"]]
    print(" ", step["relation"], step["source_id"], "->", step["destination_id"], sites)
```

```text
path False 2
app::run -> app::persist
  calls 8589934612 -> 8589934611 [('source.cpp', 13, 10), ('source.cpp', 13, 19)]
  calls 8589934611 -> 8589934610 [('source.cpp', 10, 21)]
```

A path row carries `start`, `end`, `length`, `nodes` (the full symbol rows
along the witness), and `steps`. Each step carries `relation`, `inbound`,
`source_id`, `destination_id`, and `sites`. Steps identify their endpoints
by packed symbol id, not by name; `nodes` is where the readable
`qualified_name` for each hop lives.

Each stored-relation step carries its matching source `sites` - exact call
sites, not just a symbolic edge. The first step above has two, because
`app::run` calls `app::save` twice on line 13. See
[04-views-and-catalog.md](04-views-and-catalog.md) for how site rows and
`certainty` are shaped.

### Callers/callees/paths/ancestors, side by side

```python
reachable = start(symbol("app::run")) | out("calls", 1, 3) | select(("name",))
callers_of_save = start(symbol("app::save")) | in_("calls") | select(("name",))
ancestors_of_box = start(symbol("app::Box")) | out("inherits", 1, 32) | select(("qualified_name",))

for query in (reachable, callers_of_save, ancestors_of_box):
    print(cb.executor.run(query.plan).to_dict()["rows"])
```

```text
[{'name': 'save'}, {'name': 'persist'}]
[{'name': 'run'}]
[{'qualified_name': 'app::Base'}]
```

Forward at depth 1 to 3, backward one hop, and transitive bases up to the
32-deep ceiling, respectively.

`inherits_from(target, transitive=True)` (see
[03-query-model.md](03-query-model.md)) is the predicate-level equivalent
if you want ancestry as a filter rather than as an enumeration.

## `reverse_type_use`

```python
reverse_type_use(max_depth: int = 8) -> Stage
```

`reverse_type_use` retains the cidx-compatible name but exposes only
facts-tool's **direct** stored evidence: `of_type`, return-type,
parameter-type, and template-argument-type edges. Results record which
`through` relation produced each witness. A request above direct depth
(`max_depth > 1`) returns whatever direct witnesses are available with
`partial=True` - there is no recursive, normalized type-layer traversal
the way cidx's `type_node`/`type_layer`/`type_edge` model provides; that
surface is `E_CAPABILITY` in this SDK (see
[07-error-handling.md](07-error-handling.md)).

The result is **path**-shaped. Each row pairs the declaration that uses the
type (`owner`) with the type symbol itself (`type`), and records the
relations that produced the witness in `through`:

```python
from facts_tool.queryplan import reverse_type_use

result = cb.executor.run((start(symbol("app::Box")) | reverse_type_use(1)).plan)
print(result.shape, result.partial, len(result.paths))
for row in result.paths:
    print(row["through"], row["owner"]["name"], row["owner"]["line"], row["type"]["qualified_name"])
```

```text
path False 2
['parameter_type'] box 12 app::Box
['parameter_type'] box 16 app::Box
```

Both witnesses are parameters of type `app::Box`: the `box` parameter of
`app::run` on line 12 and the `box` parameter of `app::dispatch_probe` on
line 16. Raising the depth does not find more, it only marks the answer
incomplete:

```python
print(cb.executor.run((start(symbol("app::Box")) | reverse_type_use(8)).plan).partial)
```

```text
True
```

`partial=True` at `max_depth > 1` is the honest signal that the requested
transitive type layer was not computed, not that a deeper search came back
empty.

## Witness and traversal budgets

Traversal, path, and witness reconstruction all draw on the same
`Budgets` object introduced in
[03-query-model.md](03-query-model.md): `traversal` bounds how many
neighbor-states a relation traversal can visit, `path_expansion` bounds
path search, and `witness_reconstruction` bounds how much work
reconstructing a concrete path's node sequence and sites may do. All are
overridable via `open_codebase(budgets=Budgets(...))`; hitting any of them
degrades a result to `partial=True`/`truncated=True` rather than raising,
except where a stage's own validation catches an impossible request first
(as with a depth window over `Budgets.max_depth`, which raises `E_DEPTH`
before any traversal starts).

Continue to
[06-persisted-callgraph-runs.md](06-persisted-callgraph-runs.md) for the
separate, persisted call-graph run reader - a different mechanism from the
live relation navigation covered in this chapter.
