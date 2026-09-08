# The query model

The SDK's core is a small, immutable declarative query language under
`facts_tool.queryplan`. Every constructor there returns a frozen dataclass
(`Source`, `Pred`, `TargetSet`, `Stage`, `Plan`, `Query`) with no database
handle attached - building a query never touches SQLite. This chapter
explains the model piece by piece: sources, predicates, quantifiers,
stages, how a plan executes, and how results come back. Typed graph
navigation (`GraphQuery`, `Entity`, `Callable`, `Method`, `Record`) is
covered in depth in
[05-relations-and-graph-queries.md](05-relations-and-graph-queries.md); this
chapter introduces the row-level pieces those types are built on.

## Sources: where a query starts

```python
start(source: Source | None = None) -> Query    # None defaults to codebase()
codebase() -> Source                              # enumerate everything
symbol(ref: str) -> Source                        # resolve ref against persisted symbols
entity(ref: str) -> Source                        # alias of the symbol domain
```

`entity` is adapted to the persisted symbol domain because facts-tool has
no separate entity graph the way cidx does. `Query.__or__` (`query | stage`)
is the only mutator, and it always returns a **new** `Query` - the
left-hand prefix is never mutated, so a query can be built once and reused
as a common prefix for several downstream queries.

```python
from facts_tool.queryplan import start, codebase, symbol

base = start(symbol("app::run"))
```

## Resolving a `ref`: `symbol(ref)`

A `ref` passed to `symbol(...)` is resolved by trying, in order: an exact
USR match, then an exact qualified-name match, then an exact short-spelling
match - the first non-empty match set wins.

**Ambiguous refs are not an error in the current implementation.** This
contradicts one shipped narrative doc's claim that "more than one spelling
match is an error rather than an arbitrary choice." Verified directly
against a demo project with two `app::save` overloads:

```python
from facts_tool import open_codebase
from facts_tool.queryplan import start, symbol, select

with open_codebase(facts_db="facts.sqlite", project_db="project.sqlite") as cb:
    q = start(symbol("app::save")) | select(("qualified_name", "usr", "kind"))
    for row in cb.executor.run(q.plan).to_dict()["rows"]:
        print(row)
```

```text
{'qualified_name': 'app::save', 'usr': 'c:@N@app@F@save#I#', 'kind': 'function'}
{'qualified_name': 'app::save', 'usr': 'c:@N@app@F@save#d#', 'kind': 'function'}
```

A raw plan built with `symbol(...)` returns **both** overloads as separate
nodes, in ascending persisted identity order. The typed API instead takes
the first match silently:

```python
entity = cb.get("app::save")
print(entity.qualified_name, entity.usr)
```

```text
app::save c:@N@app@F@save#I#
```

There is no ambiguity check anywhere in the source or entity resolution
code paths. If your `ref` might be overloaded, prefer an exact USR, or
build a raw plan with `symbol(ref)` and inspect every returned row yourself
rather than relying on `cb.get`/`cb.query`/`cb.find`.

## Predicates

Comparison constructors (`queryplan/predicates.py`) bind Python values -
never raw SQL:

```python
eq(field_name: str, value: Any) -> Pred
ne(field_name: str, value: Any) -> Pred
glob(field_name: str, pattern: str) -> Pred      # fnmatch.fnmatchcase, shell-style
in_list(field_name: str, values: Sequence[Any]) -> Pred
```

Boolean composition, with three-valued logic:

```python
all_of(preds: Sequence[Pred]) -> Pred   # empty all_of(()) is True
any_of(preds: Sequence[Pred]) -> Pred   # empty any_of(()) is False
not_(pred: Pred) -> Pred                # negation; unknown propagates
```

`field_name` is checked against the current view's catalog at
`validate()` time - an unknown field name fails `E_FIELD` even for a query
that would otherwise return zero rows, so a typo can never masquerade as a
valid negative result.

### Three-valued logic and unknown evidence

A field that is legitimately absent from a row's view evaluates as
**unknown**, not as an error, at predicate-evaluation time. A relation
traversal that hits the traversal budget (`truncated=True`) similarly
forces relationship quantifiers to return unknown *unless the count already
proves the answer* - `exists` short-circuits `True` the instant one match
is found, even under truncation.

```python
from facts_tool import Budgets, open_codebase
from facts_tool.queryplan import start, codebase, nodes, exists, eq

with open_codebase(
    facts_db="facts.sqlite", project_db="project.sqlite",
    budgets=Budgets(traversal=0),
) as cb:
    q = start(codebase()) | nodes(exists("calls", eq("name", "save")), unknown="include")
    r = cb.executor.run(q.plan)
    print(r.unknown, len(r.to_dict()["nodes"]))

    q2 = start(codebase()) | nodes(exists("calls", eq("name", "save")), unknown="error")
    cb.executor.run(q2.plan)
```

```text
True 32
facts_tool.errors.FactsToolError: E_UNKNOWN: predicate evidence is unknown for symbol:438
```

`Budgets(traversal=0)` forces every relation traversal to immediately
truncate, which makes it a convenient way to reproduce unknown-evidence
behavior deterministically for docs or tests.

The `unknown` policy is the third argument to `nodes`/`where` (default
`"exclude"`): drop unknown rows silently, `"include"` keep them and set
`Result.unknown = True`, or `"error"` raise `E_UNKNOWN` the moment one is
found.

## Relationship quantifiers

```python
exists(relation, target=None, min_depth=1, max_depth=1, inbound=False) -> Pred
none(relation, target=None, min_depth=1, max_depth=1, inbound=False) -> Pred
all(relation, target=None, min_depth=1, max_depth=1, inbound=False) -> Pred
at_least(threshold, relation, target=None, min_depth=1, max_depth=1, inbound=False) -> Pred
exactly(threshold, relation, target=None, min_depth=1, max_depth=1, inbound=False) -> Pred
```

`target` is itself a `Pred` applied to the neighbor found by traversing
`relation`, not a symbol ref string. `exists`/`none` also back the semantic
helper functions described below.

## Semantic helper predicates

`facts_tool.queryplan` re-exports a set of named convenience predicates
from `queryplan/helpers.py`. Two different calling conventions are mixed
in here - read the signature before reaching for one:

| Function | Signature | Target argument |
|---|---|---|
| `inherits_from` | `(target: str \| TargetSet, transitive: bool = False) -> Pred` | a symbol **ref** (or `TargetSet`) |
| `implements` | `(target: str \| TargetSet) -> Pred` | a symbol **ref** (or `TargetSet`) |
| `has_ancestor` | `(target: str, transitive: bool = True) -> Pred` | a symbol **ref** |
| `is_specialization_of` | `(target: str) -> Pred` | a symbol **ref** |
| `is_instantiation_of` | `(target: str) -> Pred` | a symbol **ref** |
| `has_member` | `(target: Pred \| None = None) -> Pred` | a **predicate** on the member |
| `has_method` | `(target: Pred \| None = None) -> Pred` | a **predicate** on the method |
| `has_field` | `(target: Pred \| None = None) -> Pred` | a **predicate** on the field |
| `has_nested` | `(target: Pred \| None = None) -> Pred` | a **predicate** on the nested symbol |
| `has_template_arg` | `(target: Pred \| None = None) -> Pred` | a **predicate** on the template argument |
| `calls` | `(target: Pred \| None = None) -> Pred` | a **predicate** on the callee |
| `called_by` | `(target: Pred \| None = None) -> Pred` | a **predicate** on the caller |
| `uses` | `(target: Pred \| None = None) -> Pred` | a **predicate** on the used symbol |
| `used_by` | `(target: Pred \| None = None) -> Pred` | a **predicate** on the user |
| `is_abstract` / `is_interface` / `is_pure` / `is_static` / `is_template` / `is_instance` | `() -> Pred` | no argument |

`has_member` is a deliberate correction versus the reference cidx
implementation: it is the OR of an inbound `field_of` and an inbound
`method_of` traversal, since facts-tool has no single combined
member-of relation.

**`is_template()` is dead code on real data.** It checks
`kind in ("class_template", "function_template")`, but the persisted
`clang::index::SymbolKind` catalog never contains those strings - a real
templated struct reports `kind == "struct"` regardless of whether it is a
template. Verified against a real project with `Box`/`Holder` templates:

```python
from facts_tool.queryplan import start, codebase, nodes, is_template, is_instance

q1 = start(codebase()) | nodes(is_template())
q2 = start(codebase()) | nodes(is_instance())
print(len(cb.executor.run(q1.plan).nodes), len(cb.executor.run(q2.plan).nodes))
```

```text
0 2
```

`is_template()` returned zero rows; `is_instance()` (which checks the
`instantiates` relation instead) correctly returned `Box` and `Holder`.
Prefer `is_instance()`, or an explicit `eq("kind", "struct")` / `glob`
check, over `is_template()`.

A `calls(...)`/`has_method(...)`-style helper called with **no** argument
matches any related node at all; called with a predicate, it filters the
related node. Two real examples:

```python
q = start(codebase()) | nodes(calls(eq("qualified_name", "app::save"))) | select(("qualified_name",))
```

```text
[{'qualified_name': 'app::run'}]
```

```python
q = start(codebase()) | nodes(has_method(eq("is_virtual", True))) | select(("qualified_name",))
```

```text
[{'qualified_name': 'app::Base'}, {'qualified_name': 'app::Box'}, {'qualified_name': 'app::Box'}]
```

(Two `app::Box` rows appear because the template pattern and its explicit
instantiation are both persisted symbols with a virtual `flush` override.)

### Every comparison, quantifier, and helper, worked

The demo database used below is one translation unit holding ten free
functions in namespace `app`, a polymorphic base `app::Base` with a pure
virtual `flush`, two class templates `app::Box` and `app::Holder`, and an
enum `app::Color`. Each snippet is the tail of a
`start(codebase()) | ... | select(...)` query; the output is the
`to_dict()["rows"]` payload.

| Query fragment | Rows returned |
|---|---|
| `nodes(glob("name", "diamond_*"))` | `[{'name': 'diamond_end'}, {'name': 'diamond_left'}, {'name': 'diamond_right'}, {'name': 'diamond_source'}]` |
| `nodes(in_list("kind", ["enum", "enum_constant"]))` | `[{'qualified_name': 'app::Color', 'kind': 'enum'}, {'qualified_name': 'app::Color::Red', 'kind': 'enum_constant'}]` |
| `nodes(all_of((eq("kind", "function"), eq("is_noexcept", True))))` | `[{'qualified_name': 'app::run'}]` |
| `nodes(any_of((eq("kind", "enum"), eq("kind", "constructor"))))` | `[{'qualified_name': 'app::Color', 'kind': 'enum'}, {'qualified_name': 'app::Box<int, 7>::Box', 'kind': 'constructor'}, {'qualified_name': 'app::Base::Base', 'kind': 'constructor'}]` |
| `nodes(eq("kind", "struct")) \| where(not_(eq("is_polymorphic", True)))` | `app::Base`, `app::Box`, `app::Holder`, `app::Box`, `app::Holder` |
| `nodes(at_least(2, "calls"))` | `[{'qualified_name': 'app::diamond_source'}]` |
| `nodes(exactly(1, "calls"))` | `app::save`, `app::run`, `app::dispatch_probe`, `app::diamond_left`, `app::diamond_right`, `app::cycle_a`, `app::cycle_b` |
| `nodes(eq("kind", "function")) \| where(none("calls"))` | `[{'name': 'persist'}, {'name': 'diamond_end'}]` |
| `nodes(exists("calls", None, 1, 1, True))` | every callee: `app::Box::flush`, `app::persist`, `app::save`, `app::diamond_end`, `app::diamond_left`, `app::diamond_right`, `app::cycle_a`, `app::cycle_b` |
| `nodes(inherits_from("app::Base"))` | `[{'qualified_name': 'app::Box'}, {'qualified_name': 'app::Box'}]` |
| `nodes(has_ancestor("app::Base"))` | same two `app::Box` rows |
| `nodes(implements("app::Base"))` | same two `app::Box` rows |
| `nodes(is_pure())` | `[{'qualified_name': 'app::Base::flush'}]` |
| `nodes(is_instantiation_of("app::Box"))` | `[{'qualified_name': 'app::Box'}]` |
| `nodes(has_field())` | `app::Box`, `app::Holder`, `app::Box`, `app::Holder` |
| `nodes(has_nested())` | `[{'qualified_name': 'app::Color'}]` |
| `nodes(has_template_arg())` | `[{'qualified_name': 'app::Box'}, {'qualified_name': 'app::Holder'}]` |
| `nodes(called_by(eq("name", "run")))` | `[{'qualified_name': 'app::save'}]` |
| `nodes(used_by(eq("name", "run")))` | `[{'qualified_name': 'app::Box<int, 7>::value'}]` |

Four more helpers return zero rows against this database, for three
different reasons. Check which one applies before concluding that an empty
result means "no such code":

- `is_static()` is genuinely empty here. The fixture has no static member,
  and the `is_static` flag is populated correctly for the symbols that do
  have one.
- `is_specialization_of("app::Box")` is empty because the database contains
  no `specializes` edges at all. Clang records the `Box<int, 7>`
  instantiation under `instantiates`, so `is_instance()` and
  `is_instantiation_of("app::Box")` are the helpers that fire.
- `is_abstract()` is empty even though `app::Base` *is* abstract: it
  declares the pure virtual `flush`. The helper is a plain
  `eq("is_abstract", True)`, and the extractor leaves the persisted
  `is_abstract` flag at `False` on this record (`is_polymorphic` is
  likewise `False` on a record with a virtual method). Use
  `has_method(is_pure())` or the `overrides` relation when you need
  abstractness, not the flag.
- `is_interface()` checks `kind in ("protocol", "interface")`. `protocol` is
  the Objective-C symbol kind and `interface` is not a persisted kind at
  all, so like `is_template()` this helper cannot match a C++ symbol.

## Target sets

```python
any_target(refs: Sequence[str]) -> TargetSet
all_targets(refs: Sequence[str]) -> TargetSet
no_targets(refs: Sequence[str]) -> TargetSet
```

Compose with `inherits_from` and the other ref-taking helpers above to
express "any of these refs", "all of these refs", or "none of these refs"
as a single target. Against the same database, with `app::Nope` a ref that
matches nothing:

| Query fragment | Rows returned |
|---|---|
| `nodes(inherits_from(any_target(["app::Base", "app::Nope"])))` | `[{'qualified_name': 'app::Box'}, {'qualified_name': 'app::Box'}]` |
| `nodes(inherits_from(all_targets(["app::Base", "app::Nope"])))` | `[]` |
| `nodes(eq("kind", "struct")) \| where(inherits_from(no_targets(["app::Base"])))` | `app::Base`, `app::Holder`, `app::Holder` |

## Stages

Stages transform the current view of nodes. Depth windows on `out`/`in_`/
`path`/`reverse_type_use` are validated against a hardcoded ceiling of 32
at plan-validation time (`E_DEPTH` if exceeded), and separately checked
against the session's own `Budgets.max_depth` at `Executor.run` time
(`E_BUDGET` if a request within 32 still exceeds a tighter configured
budget):

```python
nodes(pred=None, unknown="exclude") -> Stage    # enumerate current view, optionally filtered
where(pred, unknown="exclude") -> Stage         # filter already-enumerated nodes
view(level: str) -> Stage                       # switch catalog view; resets enumeration
out(relation, min_depth=1, max_depth=1, mode="static") -> Stage
in_(relation, min_depth=1, max_depth=1) -> Stage
sites() -> Stage                                # turn matching edge rows into site rows
select(fields: Sequence[str]) -> Stage          # nodes -> rows
distinct() -> Stage
order_by(fields: Sequence[str]) -> Stage        # nulls last
limit(n: int) -> Stage
count() -> Stage                                # terminal scalar
union_(operand: Query) -> Stage
intersect(operand: Query) -> Stage
except_(operand: Query) -> Stage
path(to: Query, relation, min_depth=1, max_depth=8, shortest=0, inbound=False) -> Stage
rank(top_n=0) -> Stage                          # only stage allowed after path besides distinct/limit/count
reverse_type_use(max_depth=8) -> Stage
```

`out(..., mode="devirtualized")` always fails at validation with
`E_CAPABILITY`; there is no devirtualization available from stored facts -
query the persisted `dispatch_calls` relation explicitly instead (see
[04-views-and-catalog.md](04-views-and-catalog.md)).

A depth window above the executor's budget fails at run time:

```python
start(symbol("app::run")) | out("calls", 1, 999)
```

```text
E_DEPTH: invalid depth window 1..999; maximum is 32
```

`sites()` requires the current view to already be `"edge"`; a following
`view("site")` is redundant and fails `E_STAGE` since `sites()` already
performed that transition:

```text
E_STAGE: view('site') cannot follow sites()
```

`union_`/`intersect`/`except_` require the operand to share the same node
view as the query it is combined with:

```text
E_SETOP: set operands must have the same node view
```

`limit(n)` and `rank(n)` both reject a negative `n` with `E_LIMIT`
(`E_LIMIT: limit value cannot be negative` / `E_LIMIT: rank value cannot be
negative`), and both accept `n == 0`. The two treat zero differently, and
the difference is easy to get wrong:

- `limit(0)` slices the result to zero rows. It does **not** mean "no
  limit". Verified: a `nodes()` enumeration that returns 32 rows returns 0
  rows once `| limit(0)` is appended.
- `rank(0)` sorts the witnesses and applies **no** cap. Verified: the
  single `app::run` to `app::persist` witness survives `| rank(0)`.

Omit the stage entirely when you want everything; do not pass `limit(0)`.

### Set operations and shaping, worked

```python
functions = start(codebase()) | nodes(eq("kind", "function"))
noexcept_functions = start(codebase()) | nodes(
    all_of((eq("kind", "function"), eq("is_noexcept", True)))
)

print(cb.executor.run((functions | union_(noexcept_functions) | count()).plan).scalar)
print(cb.executor.run((functions | intersect(noexcept_functions) | select(("name",))).plan).to_dict()["rows"])
print(cb.executor.run((functions | except_(noexcept_functions) | count()).plan).scalar)
print(cb.executor.run((start(codebase()) | nodes() | count()).plan).scalar)
print(cb.executor.run((start(codebase()) | nodes(eq("kind", "struct")) | select(("qualified_name",)) | distinct()).plan).to_dict()["rows"])
print(cb.executor.run((start(codebase()) | nodes(eq("kind", "function")) | select(("name", "line")) | order_by(("line",)) | limit(3)).plan).to_dict()["rows"])
```

```text
10
[{'name': 'run'}]
9
32
[{'qualified_name': 'app::Base'}, {'qualified_name': 'app::Box'}, {'qualified_name': 'app::Holder'}]
[{'name': 'persist', 'line': 24}, {'name': 'save', 'line': 25}, {'name': 'run', 'line': 26}]
```

`distinct()` collapses the duplicate `app::Box`/`app::Holder` rows that the
template pattern and its instantiation would otherwise both contribute.
`count()` is terminal: the result's `shape` is `"scalar"` and the payload is
`Result.scalar`, not `Result.rows`.

An `enumeration` budget small enough to truncate makes `count()` report
`None` rather than an understated number:

```python
with open_codebase(
    facts_db="facts.sqlite", project_db="project.sqlite",
    budgets=Budgets(enumeration=2),
) as cb:
    r = cb.executor.run((start(codebase()) | nodes() | count()).plan)
    print(r.scalar, r.truncated)
```

```text
None True
```

## Serialization and inspection

`canonical_json(plan)` and `plan_to_dict(plan)` walk only frozen dataclass
data - no database handle, fully portable, and independent of which
database the plan is eventually run against:

```python
from facts_tool.queryplan import start, symbol, out, canonical_json

q = start(symbol("app::run")) | out("calls", 1, 3)
print(canonical_json(q.plan))
```

```json
{"source":{"kind":"symbol","ref":"app::run"},"stages":[{"fields":[],"inbound":false,"level":"symbol","max_depth":3,"min_depth":1,"mode":"static","n":0,"op":"out","operand":null,"pred":null,"relation":"calls","unknown":"exclude"}]}
```

`plan_to_dict(plan)` is the same data as a plain Python dict, useful when
you want to inspect or rewrite a plan programmatically rather than hash it:

```python
from facts_tool.queryplan import plan_to_dict

print(plan_to_dict((start(symbol("app::run")) | out("calls", 1, 2)).plan))
```

```text
{'source': {'kind': 'symbol', 'ref': 'app::run'}, 'stages': [{'op': 'out', 'pred': None, 'level': 'symbol', 'relation': 'calls', 'mode': 'static', 'min_depth': 1, 'max_depth': 2, 'operand': None, 'fields': [], 'n': 0, 'unknown': 'exclude', 'inbound': False}]}
```

`validate(plan)` runs the same checks the executor runs, without a database
and without executing anything. It returns `None` for a valid plan and
raises the matching `FactsToolError` otherwise:

```python
from facts_tool.queryplan import validate

print(validate((start(symbol("app::run")) | out("calls", 1, 2)).plan))
validate((start(codebase()) | select(("nope",))).plan)
```

```text
None
E_FIELD: unknown symbol field(s): nope
```

`Executor.explain(plan)` validates a plan and returns a description of it
without executing it:

```python
info = cb.executor.explain(q.plan)
print(sorted(info.keys()))
print(info["shape"])
```

```text
['budgets', 'canonical_plan', 'plan', 'provenance', 'relations', 'shape', 'views']
nodes
```

## Executor, Result, and pagination

```python
class Executor:
    def __init__(self, loader, provenance, budgets: Budgets | None = None): ...
    def run(self, plan, after_id=None, result_cap=None) -> Result: ...
    def explain(self, plan) -> dict[str, Any]: ...
```

`.run` validates the plan, checks every stage's depth against
`Budgets.max_depth`, executes, then applies `result_cap` (defaulting to
`Budgets.result_cap = 1000`). Hitting the cap sets `Result.truncated = True`
and `Result.cursor` to the last kept row's `id`/`_key`.

```python
class Result:
    shape: str            # "nodes" | "rows" | "scalar" | "path"
    view: str
    values: tuple[Row, ...]
    scalar: int | None
    truncated: bool
    partial: bool
    unknown: bool
    cursor: str | None
    provenance: PairProvenance

    @property
    def nodes(self) -> tuple[Row, ...]: ...   # non-empty only when shape == "nodes"
    @property
    def rows(self) -> tuple[Row, ...]: ...    # non-empty only when shape == "rows"
    @property
    def paths(self) -> tuple[Row, ...]: ...   # non-empty only when shape == "path"
    def __iter__(self) -> Iterator[Row]: ...
    def __len__(self) -> int: ...
    def to_dict(self) -> dict[str, Any]: ...
    def to_json(self) -> str: ...
```

`.to_dict()`/`.to_json()` always include `shape`, `view`, `truncated`,
`partial`, `unknown`, `cursor`, and `provenance`, plus the shape-appropriate
payload key (`nodes`, `rows`, or `paths`, or `scalar` for a `count()`
result). A truncated `count()` returns `scalar=None`, so a truncated count
can never look falsely exact.

Real pagination, walking four functions two at a time via `after_id` +
`result_cap`:

```python
q = start(codebase()) | nodes(eq("kind", "function"))
p1 = cb.executor.run(q.plan, result_cap=2)
print(p1.truncated, p1.cursor, [r["name"] for r in p1.to_dict()["nodes"]])
p2 = cb.executor.run(q.plan, after_id=p1.cursor, result_cap=2)
print(p2.truncated, [r["name"] for r in p2.to_dict()["nodes"]])
```

```text
True 8589934611 ['persist', 'save']
True ['run', 'dispatch_probe']
```

## Budgets

```python
@dataclass
class Budgets:
    enumeration: int = 10_000
    traversal: int = 10_000
    result_cap: int = 1_000
    max_depth: int = 32
    path_expansion: int = 10_000
    witness_reconstruction: int = 200_000

    def to_dict(self) -> dict[str, int]: ...
```

Every field is overridable via `open_codebase(budgets=Budgets(...))`, and
`result_cap` additionally overridable per call to `Executor.run`.
`Budgets(traversal=0)` is the standard trick for deterministically forcing
truncation in tests or docs.

## Failure codes seen at this layer

All failures are `FactsToolError(code, message)` - a plain `Exception`
subclass with `.code`/`.message` attributes and `str(exc) == f"{code}: {message}"`.
There is exactly one exception type; callers distinguish failures by
`.code`. The full table, with when each one fires and live message text,
is in [07-error-handling.md](07-error-handling.md). The codes introduced in
this chapter are `E_SOURCE`, `E_FIELD`, `E_DEPTH`, `E_LIMIT`, `E_BUDGET`,
`E_SETOP`, `E_STAGE`, `E_UNKNOWN`, and `E_CAPABILITY`.

## The fluent, immutable `EntityQuery`

`CodeBase.query(ref=None)` (equivalently `cb.graph.query(...)`) returns an
`EntityQuery` - a lazy, immutable, chainable wrapper over the same
`Plan`/`Query` machinery:

```python
class EntityQuery:
    @property
    def plan(self) -> Plan: ...
    def to_plan(self) -> Plan: ...
    def nodes(self, pred=None, unknown="exclude") -> "EntityQuery": ...
    def where(self, pred, unknown="exclude") -> "EntityQuery": ...
    def view(self, name: str) -> "EntityQuery": ...
    def relation(self, name: str, min_depth=1, max_depth=1, inbound=False) -> "EntityQuery": ...
    def select(self, fields: Sequence[str]) -> "EntityQuery": ...
    def order_by(self, fields: Sequence[str]) -> "EntityQuery": ...
    def limit(self, value: int) -> "EntityQuery": ...
    def filter(self, callback: Callable[[Row], bool]) -> "EntityQuery": ...
    def run(self) -> Result: ...
    def all(self) -> list[object]: ...
    def names(self) -> list[str]: ...
    def count(self) -> int | None: ...
    def first(self) -> object | None: ...
```

Every chaining method (`nodes`, `where`, `view`, `relation`, `select`,
`order_by`, `limit`) returns a **new** `EntityQuery` wrapping a new `Query`
prefix, exactly like the raw `query | stage` syntax. `.relation(name,
inbound=...)` is sugar for `out`/`in_` depending on the `inbound` flag.
`.all()` upgrades node rows back into typed `Entity` objects (see
[05-relations-and-graph-queries.md](05-relations-and-graph-queries.md)).

```python
base = cb.query("app::run").relation("calls")
print(base.names())
```

```text
['save']
```

Called with no `ref`, `cb.query()` starts at `codebase()` instead of a
single symbol, and the terminals behave as follows:

```python
functions = cb.query().nodes(eq("kind", "function"))
print(functions.count())
print(type(functions.first()).__name__, functions.first().qualified_name)
print([type(x).__name__ for x in functions.all()[:3]])
print(functions.names()[:3])
print(functions.order_by(("name",)).limit(3).names())
print(functions.where(eq("is_noexcept", True)).names())
print(cb.query().view("file").nodes().select(("path",)).run().to_dict()["rows"])
```

```text
10
Callable app::persist
['Callable', 'Callable', 'Callable']
['persist', 'save', 'run']
['cycle_a', 'cycle_b', 'diamond_end']
['run']
[{'path': '.../source.cpp'}, {'path': '.../api.hpp'}]
```

`.count()` appends a `count()` stage and returns the scalar (or `None` if
the underlying enumeration truncated). `.first()` and `.all()` return typed
`Entity` subclasses; `.names()` projects the `name` field; `.run()` returns
the raw `Result`. Absolute paths in the last line are abbreviated.

### `.filter()` is local, not serializable

`.filter(callback)` stores the Python callback **outside** the plan; it
never appears in `canonical_json` and is applied client-side, after
`Executor.run` completes:

```python
from facts_tool.queryplan import canonical_json

filtered = base.filter(lambda row: row["name"] == "save")
print(filtered.names())
print(canonical_json(filtered.plan) == canonical_json(base.plan))
```

```text
['save']
True
```

The `canonical_json` of the filtered and unfiltered queries is
byte-identical - the filter is invisible to any downstream serialization
or replay, which makes it a good tool for one-off local shaping but not for
anything you need to communicate or cache as a portable query.

Continue to [04-views-and-catalog.md](04-views-and-catalog.md) for the
view/relation/kind catalogs that back every field and relation name used
above.
