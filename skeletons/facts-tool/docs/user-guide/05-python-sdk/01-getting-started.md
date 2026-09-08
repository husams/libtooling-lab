# Getting started with the Python SDK

The Python SDK is a separate, installable package named `facts-tool-query`
(import name `facts_tool`). It is a **read-only query layer**: it opens a
facts database and a project database that were already produced by the
`facts-tool` command-line tool, and lets you query them with a declarative,
immutable query language, a typed graph API, and a fluent API. It never
indexes, imports, extracts, migrates, or writes to either database, and it
never invokes Clang, libclang, or the native `facts-tool` binary.

This chapter covers installing the SDK two different ways, telling a
checkout install apart from an installed wheel, and running your first
query. Later chapters in this part cover the query model in depth; see
[03-query-model.md](03-query-model.md) onward.

## Requirements

Runtime dependencies are the Python standard library only. Supported
interpreters are CPython 3.12 and 3.13, on macOS and Linux (including
RHEL-compatible distributions).

## Installing from a checkout with uv

Inside a `facts-tool` checkout, the SDK lives under `python/` and is managed
with [uv](https://docs.astral.sh/uv/). `uv sync --locked` creates an
editable `.venv` from the committed lockfile:

```console
$ cd python && uv sync --locked
```

`python/.venv/bin/python` then has `facts_tool` importable directly against
the checkout's source tree. This is what every example in this user guide
was run against.

## Building and installing a wheel

To install the SDK the way an external consumer would - outside a
checkout, with no `PYTHONPATH` tricks - build a wheel with `uv build` and
install it into a fresh virtual environment:

```console
$ cd python && uv build --out-dir /path/to/dist
Building source distribution...
Building wheel from source distribution...
Successfully built .../facts_tool_query-0.2.0.tar.gz
Successfully built .../facts_tool_query-0.2.0-py3-none-any.whl

$ uv venv --seed --python 3.12 /path/to/install-venv
$ /path/to/install-venv/bin/python -m pip install /path/to/dist/facts_tool_query-0.2.0-py3-none-any.whl
Successfully installed facts-tool-query-0.2.0
```

Plain `pip` works against a built wheel or against the source tree:

```console
python -m pip install /path/to/dist/facts_tool_query-0.2.0-py3-none-any.whl
```

The package is not published to any index at the moment; `pip install
facts-tool-query` by bare name has nothing to resolve against. Build the
wheel from a checkout, as above.

Note that the checkout's own `.venv` is a `uv`-managed environment and does
not contain a plain `pip` executable; use `uv pip` or a separate
`uv venv --seed` environment (as above) whenever you need `pip` directly
against a `facts-tool` checkout.

## Telling a checkout apart from an installed wheel

Both installation paths report the identical version string, because both
come from the same `pyproject.toml`. To tell them apart at runtime, look at
`facts_tool.__file__` or at the installed distribution's file manifest:

```python
from importlib.metadata import version, distribution
import facts_tool

print(version("facts-tool-query"))
print(facts_tool.__file__)
print(
    "editable?",
    "direct_url.json" in [f.name for f in distribution("facts-tool-query").files],
)
```

Against the checkout's editable `.venv`:

```text
0.2.0
/Users/husam/.../facts-tool/python/src/facts_tool/__init__.py
editable? True
```

Against an isolated venv with the wheel installed:

```text
0.2.0
```

(`facts_tool.__file__` there points under that venv's
`site-packages/facts_tool/...` instead of `src/facts_tool/...`, and
`distribution(...).files` for the wheel install has no `direct_url.json`
entry.) In short: same version either way, but the `__file__` path and the
presence of `direct_url.json` tell you whether you are running the checkout
or a built artifact.

## First query

Every session needs two SQLite files that were produced together by
`facts-tool`: the facts database (symbols, relations, sites - produced by
`extract`) and the project database (repositories, components, files -
produced by `repo add` / `import`). Opening is always read-only; see
[02-opening-databases.md](02-opening-databases.md) for the full pairing and
schema-version story.

```python
from facts_tool import open_codebase
from facts_tool.queryplan import codebase, eq, nodes, select, start

with open_codebase(facts_db="facts.sqlite", project_db="project.sqlite") as cb:
    query = (
        start(codebase())
        | nodes(eq("kind", "function"))
        | select(("name", "file", "line"))
    )
    print(cb.executor.run(query.plan).to_dict())
```

Run against a small real demo database (one translation unit with ten
functions). Printing `.to_dict()["rows"]` one row per line, with absolute
paths abbreviated to `.../api.hpp`:

```text
{'name': 'persist', 'file': '.../api.hpp', 'line': 24}
{'name': 'save', 'file': '.../api.hpp', 'line': 25}
{'name': 'run', 'file': '.../api.hpp', 'line': 26}
{'name': 'dispatch_probe', 'file': '.../api.hpp', 'line': 27}
{'name': 'diamond_end', 'file': '.../api.hpp', 'line': 28}
{'name': 'diamond_left', 'file': '.../api.hpp', 'line': 29}
{'name': 'diamond_right', 'file': '.../api.hpp', 'line': 30}
{'name': 'diamond_source', 'file': '.../api.hpp', 'line': 31}
{'name': 'cycle_a', 'file': '.../api.hpp', 'line': 32}
{'name': 'cycle_b', 'file': '.../api.hpp', 'line': 33}
```

Rows come back in persisted identity order, not alphabetical order. Append
`| order_by(("name",))` if you want them sorted; that is what
`examples/basics.py` does.

`start(codebase())` begins an immutable query at the "enumerate everything"
source; `nodes(...)` filters it; `select(...)` turns matching symbol nodes
into plain rows. `cb.executor.run(query.plan)` executes the frozen plan and
returns a `Result`. Chapters
[03-query-model.md](03-query-model.md) and
[04-views-and-catalog.md](04-views-and-catalog.md) build up this language
piece by piece; chapter [08-api-reference.md](08-api-reference.md) is a
compact table of every public name if you already know roughly what you
want.

## Running the bundled examples

The SDK ships four runnable examples under `python/examples/`, each taking
`facts.sqlite` and `project.sqlite` as positional arguments and printing one
`Result.to_json()` line per query:

```console
$ uv run python examples/basics.py facts.sqlite project.sqlite
```

`basics.py`, `call_paths.py`, `facts.py`, and `project.py` are referenced
throughout this part of the guide as worked examples; see
[05-relations-and-graph-queries.md](05-relations-and-graph-queries.md) for
`call_paths.py` and [04-views-and-catalog.md](04-views-and-catalog.md) for
`project.py`.
