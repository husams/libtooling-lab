# facts-tool-query

`facts-tool-query` includes a read-only Python SDK for querying a facts-tool facts
SQLite database together with its project/configuration SQLite database. It
provides an immutable declarative query language, typed graph navigation, full
result provenance, and explicit capability errors. Direct SQLite queries do not
modify either database. Optional typed REST clients query server-owned resources
and run import, extraction, matching, call-graph, and variable-flow analysis jobs.

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

## REST clients

From the repository root, install the optional HTTP client dependency:

```console
python -m pip install './skeletons/facts-tool/python[rest]'
```

```python
from facts_tool.rest import AsyncClient

async def inspect_server():
    async with AsyncClient("http://127.0.0.1:8080") as api:
        async for symbol in api.symbols.find(qualified_name="app::"):
            print(symbol.qualified_name)
```

`Client` provides the same methods synchronously. Pass `token=` explicitly for
authenticated servers. See [REST SDK usage](../docs/user-guide/05-python-sdk/11-rest-client.md),
[installation](../docs/user-guide/09-rest-api/05-installation.md), and
[deployment](../docs/user-guide/09-rest-api/06-deployment.md).

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
RHEL-compatible distributions. The direct SQLite SDK uses only Python's standard
library and never invokes Clang, libclang, cpp-indexer, or facts-tool. REST clients
require the optional HTTPX dependency and submit CLI jobs to an existing server.

See [Typed REST API v2](docs/rest-v2.md) for resource clients, automatic import,
lazy results, and async usage.
