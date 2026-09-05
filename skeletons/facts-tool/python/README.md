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
opens each with SQLite `mode=ro` and immutable semantics, validates its role,
and refuses unsupported facts schema versions or missing FileId mappings.

## Documentation

- [Quickstart](docs/quickstart.md)
- [Language sources and predicates](docs/language-predicates.md)
- [Language stages and paths](docs/language-stages.md)
- [Relations](docs/relations.md) and [views](docs/views.md)
- [Database lifecycle and mapping](docs/databases.md)
- [Results, errors, and budgets](docs/results.md)
- [Typed and fluent APIs](docs/model-api.md)
- [cpp-indexer compatibility](docs/cidx-migration.md)
- [UV development and validation](docs/development.md)
- [Architecture](docs/architecture.md) and [troubleshooting](docs/troubleshooting.md)
- [Cookbook](docs/cookbook.md) and runnable [examples](examples/)

Python 3.12 and 3.13 are supported on macOS and Linux, including
RHEL-compatible distributions. The installed runtime uses only Python's
standard library and never invokes Clang, libclang, cpp-indexer, or facts-tool.
