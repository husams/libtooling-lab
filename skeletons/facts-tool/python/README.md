# facts-tool-query

`facts-tool-query` is the read-only Python SDK for querying a facts-tool facts
SQLite database together with its project/configuration SQLite database. It
provides an immutable declarative query language, typed graph navigation, full
result provenance, and explicit capability errors. It does not index, import,
extract, migrate, backfill, or modify either database.

## Install and query

```console
python -m pip install facts-tool-query
```

```python
from facts_tool import open_codebase
from facts_tool.queryplan import in_, select, start, symbol

with open_codebase(facts_db="facts.sqlite", project_db="project.sqlite") as cb:
    query = start(symbol("app::run")) | in_("calls") | select(["name", "file"])
    print(cb.executor.run(query.plan).to_dict())
```

Both paths are mandatory and must identify different existing files. The SDK
opens each with SQLite `mode=ro` and query-only semantics, validates its role,
and refuses unsupported facts schema versions or missing FileId mappings.

## Lazy and eager queries

Queries are lazy by default. Keep the codebase open while consuming rows:

```python
from facts_tool.queryplan import eq

with open_codebase(facts_db="facts.sqlite", project_db="project.sqlite") as cb:
    for function in cb.query().nodes(eq("kind", "function")):
        print(function.name)
    saved = cb.query().nodes(eq("name", "run")).run(lazy=False)
print(saved.to_dict())
```

Use `executor.run(plan, lazy=False)` or `EntityQuery.run(lazy=False)` for one
eager query, or `open_codebase(..., lazy=False)` to change the default.
`Result.values`, shape-specific collections, `len`, serialization, and
`materialize()` collect the result; fluent `all()` and `names()` return lists.
See [result lifecycle and streaming limits](docs/results.md) before processing
large results.

## Documentation

- [Quickstart](docs/quickstart.md)
- [Language sources and predicates](docs/language-predicates.md)
- [Language stages and paths](docs/language-stages.md)
- [Relations](docs/relations.md), [views](docs/views.md), and [symbol kinds](docs/symbol-kinds.md)
- [Database lifecycle and mapping](docs/databases.md)
- [Results, errors, and budgets](docs/results.md)
- [Query performance and index audit](docs/query-performance.md)
- [Native matcher results](docs/match-results.md)
- [Persisted call-graph runs](docs/callgraph-runs.md)
- [Variable-flow runs](docs/variable-flow.md)
- [Expressions, field effects, and bounded source regions](docs/evidence-api.md)
- [Installed agent workflow acceptance](docs/s032-progress.md)
- [Typed and fluent APIs](docs/model-api.md)
- [cpp-indexer compatibility](docs/cidx-migration.md)
- [UV development and validation](docs/development.md)
- [Architecture](docs/architecture.md) and [troubleshooting](docs/troubleshooting.md)
- [Cookbook](docs/cookbook.md) and runnable [examples](examples/)

Python 3.12 and 3.13 are supported on macOS and Linux, including
RHEL-compatible distributions. The installed runtime uses only Python's
standard library and never invokes Clang, libclang, cpp-indexer, or facts-tool.
