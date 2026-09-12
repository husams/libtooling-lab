# How to query C++ code

The distribution is `facts-tool-query`; the import namespace is `facts_tool`.
The SDK reads a facts database and its separate project database, and can read
native match-result JSON without opening either store. It never imports,
extracts, migrates, or writes either database.

## Required access boundary

Always query persisted evidence through the public `facts_tool` Python SDK.
Never open either database with `sqlite3`, another database driver, SQL,
private SDK connections, or direct table inspection, even for diagnostics.
Use `open_codebase` and public query, model, project-view, and callgraph APIs.
If a public API is missing or rejects the store, report the capability or
schema error; do not bypass it.

To search **source code**, use native `facts-tool match --matcher` with the
Clang dynamic AST matcher DSL, following
[Source-symbol search](how-to-search-symbol.md). The SDK queries persisted
evidence; it does not run Clang matchers or extract missing source facts.

For the exact bindings returned by one native invocation, use
`load_match_results("matches.json")` or `MatchResults.from_json(stdout)` after
checking command success. Follow [match result processing](match-results.md)
for capability checks, physical coordinates, byte ranges, and publication
semantics. Do not reconstruct the invocation by querying a persistent index or
guessing that `cb.match()` exists; the result reader does not execute matchers.

## Start with a concrete symbol

Resolve the database pair from the existing YAML configuration using native
`facts-tool config show` and the
[configuration guide](../../../../docs/user-guide/02-projects-and-configuration/03-configuration-files.md);
do not ask the user to supply paths already configured. Native commands use
these defaults without database-path flags. The current `open_codebase`
Python signature still requires `facts_db` and `project_db`: pass the
resolved concrete paths as `facts_path` and `project_path` below, not raw
templates. Do not invent an `open_codebase(config=...)` parameter or assume
that omitting its required arguments loads YAML.

```python
from facts_tool import open_codebase
from facts_tool.queryplan import in_, select, start, symbol

with open_codebase(facts_db=facts_path, project_db=project_path) as cb:
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
with open_codebase(facts_db=facts_path, project_db=project_path) as cb:
    run = cb.get("app::run")
    for callee in run.callees(max_depth=3):
        print(callee.name, callee.file, callee.line)
```

## Build an evidence-backed answer

Record the exact query, matching qualified names or USRs, source locations,
relation direction and depth, and relevant call/reference sites. Check
`result.truncated`, `result.partial`, `result.unknown`, and
`result.provenance` before drawing a conclusion. Report `FactsToolError.code`
when stored facts cannot answer the question, then narrow the claim or use
the public SDK's bounded source-region APIs at the returned identity. For
missing facts, run targeted native extraction and re-query through the SDK.

Use the maintained [relation](../../../../python/docs/relations.md),
[view](../../../../python/docs/views.md), [symbol kind](../../../../python/docs/symbol-kinds.md),
[predicate](../../../../python/docs/language-predicates.md), and
[stage](../../../../python/docs/language-stages.md) catalogs. The
[model API](../../../../python/docs/model-api.md),
[results](../../../../python/docs/results.md), and
[troubleshooting](../../../../python/docs/troubleshooting.md) guides define
typed navigation and confidence limits.
