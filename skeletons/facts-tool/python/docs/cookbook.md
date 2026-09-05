# Cookbook

Find callers with declaration locations:

```python
query = start(symbol("app::save")) | in_("calls")
query |= select(("name", "file", "line"))
```

Find records that have a pure method:

```python
query = start(codebase()) | nodes(has_method(eq("is_pure", True)))
```

Reuse a prefix for shared callees and paths:

```python
reachable = start(symbol("app::run")) | out("calls", 1, 3)
shared = reachable | intersect(start(symbol("app::save")) | out("calls", 1, 3))
witness = start(symbol("app::run")) | path(
    start(symbol("app::persist")), "calls") | rank(5)
```

Inspect parameters and conventional template terminology:

```python
params = start(symbol("app::run")) | out("has_parameter")
slots = start(symbol("app::Box")) | out("has_template_parameter")
args = start(symbol("app::Box<int>")) | out("has_template_argument")
```

Inspect compile configuration and includes:

```python
files = start(codebase()) | view("file") | nodes()
files |= select(("id", "path", "driver", "compile_options"))
includes = (start(codebase()) | view("file")
            | nodes(glob("path", "*main.cpp")) | out("includes"))
```

Inspect repeated call evidence:

```python
sites_query = (start(codebase()) | view("edge")
               | nodes(eq("kind", "calls")) | sites())
```

The scripts in `examples/` run these patterns against the same small paired
fixture used by executable tests.
