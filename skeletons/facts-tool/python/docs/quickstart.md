# Quickstart

Build artifacts install with ordinary pip and require Python 3.12 or 3.13:

```console
uv sync --locked
uv build
python -m pip install dist/facts_tool_query-0.1.0-py3-none-any.whl
```

Every session needs the extracted facts database and the project/configuration
database that owns its FileIds:

```python
from facts_tool import open_codebase
from facts_tool.queryplan import codebase, eq, nodes, select, start

with open_codebase(facts_db="facts.sqlite", project_db="project.sqlite") as cb:
    query = (start(codebase()) | nodes(eq("kind", "function"))
             | select(("name", "file", "line")))
    print(cb.executor.run(query.plan).to_dict())
```

The paths must exist, name different physical files, and may contain Unicode or
spaces. Opening is read-only. The SDK performs no discovery from facts-tool
configuration files and never invokes a compiler or native executable.

Use `cb.get(ref)` for a typed object, `cb.query(ref)` for a fluent chain, or
build an immutable query under `facts_tool.queryplan`. A `ref` is resolved in
USR, exact qualified-name, then unqualified spelling order. More than one
spelling match is an error rather than an arbitrary choice.

Run [the examples](../examples/) with the two database paths:

```console
uv run python examples/basics.py facts.sqlite project.sqlite
```
