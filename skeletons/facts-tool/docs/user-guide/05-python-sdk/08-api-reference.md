# API reference

A compact index of every public name. `facts_tool` (the top-level package)
and `facts_tool.queryplan` are the two public import surfaces; nothing
under `facts_tool.budgets`, `facts_tool.ids`, `facts_tool.errors`, or any
other submodule is meant to be imported directly except where noted below
(`Budgets`, `SymbolId`, and `FactsToolError` are re-exported at the top
level for convenience).

## `facts_tool` (top level)

| Name | Signature | Purpose | Chapter |
|---|---|---|---|
| `open_codebase` | `(*, facts_db, project_db, budgets=None) -> CodeBase` | Open a paired facts/project database read-only | [02](02-opening-databases.md) |
| `CodeBase` | `.executor`, `.provenance`, `.graph`, `.callgraphs`, `.get/.find/.query`, context manager, `.close()` | The session object returned by `open_codebase` | [02](02-opening-databases.md) |
| `Executor` | `(loader, provenance, budgets=None)`; `.run(plan, after_id=None, result_cap=None) -> Result`; `.explain(plan) -> dict` | Runs a frozen `Plan` against a paired database | [03](03-query-model.md) |
| `Result` | `.shape/.view/.values/.scalar/.truncated/.partial/.unknown/.cursor/.provenance`; `.nodes/.rows/.paths`; `.to_dict()/.to_json()` | The outcome of running a plan | [03](03-query-model.md) |
| `Budgets` | `enumeration=10_000, traversal=10_000, result_cap=1_000, max_depth=32, path_expansion=10_000, witness_reconstruction=200_000` | Explicit, opt-in limits on every executor operation | [03](03-query-model.md) |
| `GraphQuery` | `.get/.find/.query/.neighbors/.reaches/.callers/.callees/.bases/.subclasses/.members/.parameters/.definitions/.references` | Typed graph navigation | [05](05-relations-and-graph-queries.md) |
| `Entity` | row-proxying `__getattr__`; `.to_dict()/.outgoing()/.incoming()/.definitions()/.references()` | Base typed wrapper over a symbol row | [05](05-relations-and-graph-queries.md) |
| `Callable` | `Entity` + `.callers()/.callees()/.parameters()` | Typed wrapper for a function/method symbol | [05](05-relations-and-graph-queries.md) |
| `Method` | `Callable` + `.record()` | Typed wrapper for a method symbol | [05](05-relations-and-graph-queries.md) |
| `Record` | `Entity` + `.bases()/.subclasses()/.methods()/.fields()` | Typed wrapper for a struct/class symbol | [05](05-relations-and-graph-queries.md) |
| `SymbolId` | `(file_id: int, index: int)`; `.packed/.sqlite/.to_dict()`; `SymbolId.unpack(value)` | Two-half packed symbol identity | [02](02-opening-databases.md) |
| `FactsToolError` | `(code: str, message: str)`; `.code/.message` | The single public exception type | [07](07-error-handling.md) |
| `CallGraphRun` | see [06](06-persisted-callgraph-runs.md) field table | One persisted `analyse call-graph` invocation | [06](06-persisted-callgraph-runs.md) |
| `CallGraphPage[T]` | `.items/.total/.next_cursor/.complete/.truncated`; iterable/sized/indexable | One bounded page of a run's child collection | [06](06-persisted-callgraph-runs.md) |
| `CallGraphEdge` | `source, target, kind_id, kind, semantic_kind, position, file_id, file, line, column, offset, depth, cycle, site` | One persisted call-graph edge | [06](06-persisted-callgraph-runs.md) |
| `CallGraphSite` | `file_id, file, line, column, offset, receiver_type_id, certainty, enriched` | Source evidence for a persisted edge | [06](06-persisted-callgraph-runs.md) |
| `CallGraphSymbol` | `symbol_id, usr, qualified_name, file_id, file, line, column` | A symbol as referenced from a persisted run | [06](06-persisted-callgraph-runs.md) |

`CodeBase.callgraphs` is a `CallGraphReader` (`.list()/.latest()/.get()`);
it is not itself re-exported at the top level - access it only through an
open `CodeBase`. Likewise `CallGraphRoot`/`CallGraphTarget`/
`CallGraphFrontier`/`CallGraphRecovery` are reachable only as fields of a
`CallGraphRun`, not as standalone top-level imports.

## `facts_tool.queryplan`

### Sources

| Name | Signature | Purpose |
|---|---|---|
| `start` | `(source: Source \| None = None) -> Query` | Begin an immutable query; `None` defaults to `codebase()` |
| `codebase` | `() -> Source` | Enumerate every persisted node |
| `symbol` | `(ref: str) -> Source` | Resolve `ref` against persisted symbols (USR, then qualified name, then spelling) |
| `entity` | `(ref: str) -> Source` | Alias of the symbol domain |

### Predicates

| Name | Signature | Purpose |
|---|---|---|
| `eq` | `(field_name: str, value: Any) -> Pred` | Field equals a bound value |
| `ne` | `(field_name: str, value: Any) -> Pred` | Field does not equal a bound value |
| `glob` | `(field_name: str, pattern: str) -> Pred` | Shell-style (`fnmatch.fnmatchcase`) match |
| `in_list` | `(field_name: str, values: Sequence[Any]) -> Pred` | Field is a member of `values` |
| `all_of` | `(preds: Sequence[Pred]) -> Pred` | Three-valued AND; empty tuple is `True` |
| `any_of` | `(preds: Sequence[Pred]) -> Pred` | Three-valued OR; empty tuple is `False` |
| `not_` | `(pred: Pred) -> Pred` | Negation; unknown propagates |

### Quantifiers

| Name | Signature | Purpose |
|---|---|---|
| `exists` | `(relation, target=None, min_depth=1, max_depth=1, inbound=False) -> Pred` | At least one matching neighbor |
| `none` | `(relation, target=None, min_depth=1, max_depth=1, inbound=False) -> Pred` | No matching neighbor |
| `all` | `(relation, target=None, min_depth=1, max_depth=1, inbound=False) -> Pred` | Every neighbor matches (vacuously true if none) |
| `at_least` | `(threshold, relation, target=None, min_depth=1, max_depth=1, inbound=False) -> Pred` | At least `threshold` matching neighbors |
| `exactly` | `(threshold, relation, target=None, min_depth=1, max_depth=1, inbound=False) -> Pred` | Exactly `threshold` matching neighbors |

### Semantic helpers

| Name | Signature | Purpose |
|---|---|---|
| `inherits_from` | `(target: str \| TargetSet, transitive=False) -> Pred` | Inherits from a symbol ref, optionally transitively |
| `implements` | `(target: str \| TargetSet) -> Pred` | Same as `inherits_from`, non-transitive; no separate interface edge exists |
| `has_ancestor` | `(target: str, transitive=True) -> Pred` | Alias of `inherits_from` with `transitive` defaulted `True` |
| `has_member` | `(target: Pred \| None = None) -> Pred` | Has a matching field or method (OR of `field_of`/`method_of` inbound) |
| `has_method` | `(target: Pred \| None = None) -> Pred` | Has a matching method |
| `has_field` | `(target: Pred \| None = None) -> Pred` | Has a matching field |
| `has_nested` | `(target: Pred \| None = None) -> Pred` | Has a matching nested symbol via `contains` |
| `has_template_arg` | `(target: Pred \| None = None) -> Pred` | Has a matching supplied template argument |
| `is_specialization_of` | `(target: str) -> Pred` | Specializes the named template |
| `is_instantiation_of` | `(target: str) -> Pred` | Instantiates the named template |
| `calls` | `(target: Pred \| None = None) -> Pred` | Calls a matching callee |
| `called_by` | `(target: Pred \| None = None) -> Pred` | Is called by a matching caller |
| `uses` | `(target: Pred \| None = None) -> Pred` | Uses a matching symbol |
| `used_by` | `(target: Pred \| None = None) -> Pred` | Is used by a matching symbol |
| `is_abstract` | `() -> Pred` | `eq("is_abstract", True)`; the flag is not populated for C++ records - see [03](03-query-model.md), prefer `has_method(is_pure())` |
| `is_interface` | `() -> Pred` | `kind in ("protocol", "interface")`; **cannot match a C++ symbol** - see [03](03-query-model.md) |
| `is_pure` | `() -> Pred` | Symbol is pure virtual |
| `is_static` | `() -> Pred` | Symbol is static |
| `is_template` | `() -> Pred` | **Dead on real data** - see [03](03-query-model.md); prefer `is_instance()` |
| `is_instance` | `() -> Pred` | Symbol is a template instantiation (checks `instantiates`) |

### Target sets

| Name | Signature | Purpose |
|---|---|---|
| `any_target` | `(refs: Sequence[str]) -> TargetSet` | Match any of the given refs |
| `all_targets` | `(refs: Sequence[str]) -> TargetSet` | Match all of the given refs |
| `no_targets` | `(refs: Sequence[str]) -> TargetSet` | Match none of the given refs |

### Stages

| Name | Signature | Purpose |
|---|---|---|
| `nodes` | `(pred=None, unknown="exclude") -> Stage` | Enumerate the current view |
| `where` | `(pred, unknown="exclude") -> Stage` | Filter already-enumerated nodes |
| `view` | `(level: str) -> Stage` | Switch catalog view |
| `out` | `(relation, min_depth=1, max_depth=1, mode="static") -> Stage` | Forward relation traversal |
| `in_` | `(relation, min_depth=1, max_depth=1) -> Stage` | Backward relation traversal |
| `sites` | `() -> Stage` | Turn matching edge rows into `site` rows |
| `union_` | `(operand: Query) -> Stage` | Set union with another plan (same node view required) |
| `intersect` | `(operand: Query) -> Stage` | Set intersection |
| `except_` | `(operand: Query) -> Stage` | Set difference |
| `path` | `(to, relation, min_depth=1, max_depth=8, shortest=0, inbound=False) -> Stage` | Deterministic shortest simple witness(es) |
| `rank` | `(top_n=0) -> Stage` | Order path results by `(length, logical id)` |
| `reverse_type_use` | `(max_depth=8) -> Stage` | Direct type-use edges only (`of_type`/return/param/template-arg) |
| `select` | `(fields: Sequence[str]) -> Stage` | Nodes to rows |
| `distinct` | `() -> Stage` | Deduplicate rows |
| `order_by` | `(fields: Sequence[str]) -> Stage` | Sort, nulls last |
| `limit` | `(n: int) -> Stage` | Truncate output |
| `count` | `() -> Stage` | Terminal scalar count |

### Serialization and validation

| Name | Signature | Purpose |
|---|---|---|
| `canonical_json` | `(plan: Plan) -> str` | Deterministic, sorted-key JSON of a frozen plan |
| `plan_to_dict` | `(plan: Plan) -> dict` | Frozen plan as a plain dict |
| `validate` | `(plan: Plan) -> None` | Raise the appropriate `FactsToolError` for an invalid plan |

### Types

| Name | Purpose |
|---|---|
| `Query` | Immutable `Plan` builder; `query \| stage` returns a new `Query` |
| `Plan` | Frozen `(source, stages)` - the portable intermediate representation |
| `Source` | Frozen `(kind, ref)` |
| `Pred` | Frozen predicate node |
| `Stage` | Frozen stage node |
| `TargetSet` | Frozen `(kind, refs)` |
| `TraversalMode` | `StrEnum`: `STATIC = "static"`, `DEVIRTUALIZED = "devirtualized"` |
| `UnknownPolicy` | `StrEnum`: `EXCLUDE = "exclude"`, `INCLUDE = "include"`, `ERROR = "error"` |

## Fluent API (accessed via `CodeBase.query()`/`GraphQuery.query()`)

Every one of these is worked through in [03](03-query-model.md).

| Name | Signature | Purpose |
|---|---|---|
| `EntityQuery.nodes` | `(pred=None, unknown="exclude") -> EntityQuery` | Enumerate, optionally filtered |
| `EntityQuery.where` | `(pred, unknown="exclude") -> EntityQuery` | Filter enumerated nodes |
| `EntityQuery.view` | `(name: str) -> EntityQuery` | Switch catalog view |
| `EntityQuery.relation` | `(name, min_depth=1, max_depth=1, inbound=False) -> EntityQuery` | Sugar for `out`/`in_` |
| `EntityQuery.select` | `(fields: Sequence[str]) -> EntityQuery` | Nodes to rows |
| `EntityQuery.order_by` | `(fields: Sequence[str]) -> EntityQuery` | Sort, nulls last |
| `EntityQuery.limit` | `(value: int) -> EntityQuery` | Truncate output |
| `EntityQuery.filter` | `(callback: Callable[[Row], bool]) -> EntityQuery` | Local, never serialized |
| `EntityQuery.plan` / `.to_plan()` | `-> Plan` | Expose the frozen plan |
| `EntityQuery.run` | `() -> Result` | Execute |
| `EntityQuery.all` | `() -> list[object]` | Execute; upgrade node rows to typed `Entity` objects |
| `EntityQuery.names` | `() -> list[str]` | Execute; project `name` field |
| `EntityQuery.count` | `() -> int \| None` | Execute as a scalar count |
| `EntityQuery.first` | `() -> object \| None` | Execute; first result or `None` |

Every chapter in this part cross-links back into this table where a name
first appears in detail: [01](01-getting-started.md) (install/first
query), [02](02-opening-databases.md) (`open_codebase`/`CodeBase`/
`SymbolId`), [03](03-query-model.md) (`queryplan` core, `Executor`,
`Result`, `Budgets`, `EntityQuery`), [04](04-views-and-catalog.md) (views/
kinds/relations), [05](05-relations-and-graph-queries.md) (`GraphQuery`/
`Entity`/`Callable`/`Method`/`Record`), [06](06-persisted-callgraph-runs.md)
(`CallGraphReader` and every `CallGraph*` model), and
[07](07-error-handling.md) (`FactsToolError` and every code).
