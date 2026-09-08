# Error handling

The SDK exposes exactly **one** exception type. Every documented failure -
schema mismatches, bad refs, invalid plans, exhausted budgets - is a
`FactsToolError` distinguished by a stable string `.code`, never a
subclass hierarchy.

## `FactsToolError`

```python
class FactsToolError(Exception):
    def __init__(self, code: str, message: str):
        self.code = code
        self.message = message
        super().__init__(f"{code}: {message}")
```

`str(exc) == f"{code}: {message}"`. Internally, every raise site in the
package goes through a single helper, `errors.fail(code, message) -> Never`
- there is exactly one exception type exported; callers distinguish
failures by `.code`, not by `isinstance` on a subclass.

```python
from facts_tool import FactsToolError

try:
    cb.get("app::nope")
except FactsToolError as exc:
    print(exc.code)
    print(exc.message)
    print(str(exc))
```

```text
E_SOURCE
symbol 'app::nope' was not found
E_SOURCE: symbol 'app::nope' was not found
```

Every `Live message text` cell in the table below is the `str(exc)` form,
which is what an uncaught traceback shows.

## Every error code, when it fires, and its live message

| Code | Fires when | Live message text (verified) |
|---|---|---|
| `E_SOURCE` | A `symbol(ref)` resolves to nothing, or an unknown plan source kind is used, or a nonexistent call-graph `run_id` is requested | `E_SOURCE: symbol 'app::Box<int>' was not found` (a ref that only matches the SDK's synthetic test fixture, tried against a real Clang-parsed database) |
| `E_VIEW` | `view(name)` names a view outside the facts/project view catalogs | `E_VIEW: unknown view 'not_a_view'` |
| `E_FIELD` | `select`/`order_by`/a predicate names a field outside the current view's catalog | `E_FIELD: unknown symbol field(s): not_a_field` |
| `E_KIND` | `out(relation, mode=...)` is given a mode other than `"static"`/`"devirtualized"` | `E_KIND: unknown traversal mode 'bogus'` |
| `E_RELATION` | `out`/`in_`/`path` names a relation outside the stored-relation and pseudo-relation catalogs | `E_RELATION: unknown relation 'has_field'` (a `queryplan.helpers` predicate name mistakenly used as a relation) |
| `E_DEPTH` | A depth window on `out`/`in_`/`path`/`reverse_type_use` exceeds a hardcoded ceiling of 32 (checked independently of `Budgets`) | `E_DEPTH: invalid depth window 1..999; maximum is 32` |
| `E_LIMIT` | A non-positive `limit`/`result_cap`, a negative `limit(n)`/`rank(n)` stage value, or a negative/non-`int`/`bool` `after`/`offset`/`cursors` value passed to `CallGraphReader` | `E_LIMIT: result_cap must be positive` |
| `E_BUDGET` | A stage requests a depth within the hardcoded 32-deep ceiling but above the session's own (possibly tighter) `Budgets.max_depth` | `E_BUDGET: plan depth exceeds the executor budget` (reproduced with `Budgets(max_depth=2)` and a `max_depth=5` traversal) |
| `E_SETOP` | `union_`/`intersect`/`except_` operands do not share the same node view | `E_SETOP: set operands must have the same node view` |
| `E_STAGE` | A stage appears in an invalid position - e.g. a stage after `select`/`count`/`path` that isn't allowed there, `view("site")` right after `sites()`, `sites()` off a non-`edge` view, or a `union_`/`intersect`/`except_`/`path` missing its operand | `E_STAGE: view('site') cannot follow sites()` |
| `E_UNKNOWN` | A predicate's `unknown="error"` policy is hit because the evidence is genuinely unprovable (absent field, or a traversal that hit its budget without resolving the quantifier) | `E_UNKNOWN: predicate evidence is unknown for symbol:438` |
| `E_CAPABILITY` | A request needs semantics the stored facts (or current schema) do not carry: `mode="devirtualized"` traversal, `cb.callgraphs.*` on a schema below 12, or any cidx entity/type-layer/call-argument-graph surface this package does not implement | `E_CAPABILITY: persisted call graph runs require facts schema 12` |
| `E_DATABASE` | `open_codebase` is given a path that does not exist, is not a regular file, or cannot be opened read-only | `E_DATABASE: facts database does not exist: <path>` |
| `E_DATABASE_ROLE` | The two database arguments name the same physical file, or a database's required tables don't match its claimed role (e.g. facts/project arguments swapped) | `E_DATABASE_ROLE: facts and project databases must be different files` |
| `E_SCHEMA` | An unsupported facts `user_version`, or a schema-12-shaped database missing required `callgraph_run*` tables/columns | `E_SCHEMA: facts schema user_version 5 is unsupported; need 10, 11, or 12` |
| `E_DATABASE_PAIR` | A `FileId` used by the facts database's `symbol`/`definition`/`relation_site`/`include_dependency` tables is absent from the paired project database's `file` table | `E_DATABASE_PAIR: project database lacks FileIds: [2]` |
| `E_IDENTITY` | A `SymbolId` is constructed with an out-of-range 32-bit half, or a `FileId` required to resolve is absent from the project database | `E_IDENTITY: symbol identity halves must be unsigned 32-bit` |

Every one of these was reproduced live, in this session, against a real or
hand-built database.

## Reading a failure defensively

```python
from facts_tool import FactsToolError, open_codebase

try:
    with open_codebase(facts_db=facts_path, project_db=project_path) as cb:
        result = cb.executor.run(query.plan)
except FactsToolError as exc:
    match exc.code:
        case "E_SCHEMA":
            ...  # database predates the feature you need; re-extract, don't retry
        case "E_DATABASE_PAIR":
            ...  # wrong project database for this facts database
        case "E_SOURCE":
            ...  # nothing matched the ref; an ambiguous ref does not land here
        case _:
            raise
```

`E_SOURCE` is worth special attention: it always means "nothing matched the
ref," never "too many things matched." Ambiguous refs are **not** rejected
(see [03-query-model.md](03-query-model.md)); a raw plan returns every match
and `cb.get`/`cb.query` silently take the first. So catching `E_SOURCE` does
not protect you from an overloaded name resolving to the wrong symbol. Pass
an exact USR when you need to be certain a ref names exactly one symbol.

## Truncation and unknown are not exceptions

A budget-truncated result does not raise. It sets `Result.truncated = True`
(and, for a `count()`, `scalar = None` so a truncated count can never look
falsely exact) or `Result.unknown = True` for a predicate whose evidence
couldn't be proven under the `"include"` unknown policy. Only the
`unknown="error"` policy converts an unprovable predicate into an
`E_UNKNOWN` exception. Treat a truncated or unknown result as "the answer
is incomplete," never as "the answer is false" - a truncated empty result
is not proof of absence.

Continue to [08-api-reference.md](08-api-reference.md) for a compact,
per-module index of every public name in the package.
