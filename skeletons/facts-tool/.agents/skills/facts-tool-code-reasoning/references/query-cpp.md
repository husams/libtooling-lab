# How to query C++ code

The distribution is `facts-tool-query`; the import namespace is `facts_tool`.
The SDK reads a facts database and its separate project database. It never
imports, extracts, migrates, or writes either file.

## Start with a concrete symbol

```python
from facts_tool import open_codebase
from facts_tool.queryplan import in_, select, start, symbol

with open_codebase(facts_db="facts.sqlite", project_db="project.sqlite") as cb:
    query = start(symbol("app::save")) | in_("calls")
    query |= select(("name", "kind", "file", "line"))
    result = cb.executor.run(query.plan)
    print(result.to_json())
```

Prefer a USR or exact qualified name. An ambiguous unqualified spelling raises
an error instead of choosing an arbitrary declaration.

## Choose the API for the question

- Declarative plans: compose `start`, `nodes`, `where`, `out`, `in_`, `path`,
  `select`, `order_by`, `distinct`, `count`, and `limit`.
- Typed navigation: use `cb.get(ref)` and methods such as `callers`, `callees`,
  `bases`, `subclasses`, `members`, `parameters`, `definitions`, and
  `references`.
- Fluent navigation: use `cb.query(ref).relation(...).select(...).run()`.
- Project context: select `view("file")`, `view("component")`, or another
  project view to inspect paths, drivers, compile options, and ownership.

```python
with open_codebase(facts_db="facts.sqlite", project_db="project.sqlite") as cb:
    run = cb.get("app::run")
    for callee in run.callees(max_depth=3):
        print(callee.name, callee.file, callee.line)
```

## Build an evidence-backed answer

Record the exact query, matching qualified names or USRs, source locations,
relation direction and depth, and relevant call/reference sites. Check
`result.truncated`, `result.partial`, `result.unknown`, and
`result.provenance` before drawing a conclusion. Report `FactsToolError.code`
when stored facts cannot answer the question, then narrow the claim or inspect
source at the returned locations.

Use the maintained [relation](../../../../python/docs/relations.md),
[view](../../../../python/docs/views.md), [symbol kind](../../../../python/docs/symbol-kinds.md),
[predicate](../../../../python/docs/language-predicates.md), and
[stage](../../../../python/docs/language-stages.md) catalogs. The
[model API](../../../../python/docs/model-api.md),
[results](../../../../python/docs/results.md), and
[troubleshooting](../../../../python/docs/troubleshooting.md) guides define
typed navigation and confidence limits.
