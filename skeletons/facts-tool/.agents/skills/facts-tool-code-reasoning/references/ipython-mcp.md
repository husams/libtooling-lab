# How to deploy in IPython-MCP

Use the installed `ipython-mcp` skill for the runtime contract. Do all Python
execution through IPython-MCP; do not invoke Python, pip, or subprocesses from a
shell.

## Install the package

1. Call `runtime_status` and continue only when the runtime is ready.
2. Use `search` or `inspect` to check for an existing `facts_tool` session.
3. Have the project's UV workflow produce the wheel, then install that artifact
   in `execute` with IPython's pip magic:

```python
%pip install "/absolute/path/to/facts_tool_query-0.1.0-py3-none-any.whl"
```

For active source development, `%pip install -e
"/absolute/path/to/libtooling-lab/skeletons/facts-tool/python"` is also valid.
Invalidate import caches and import `facts_tool`. An editable install may
require a worker restart before its new path is visible; if import still fails,
stop and report that requirement. Package locking, building, and validation
remain defined in the [development guide](../../../../python/docs/development.md)
and use UV.

## Keep the paired databases open

Run this through `execute`, replacing any previous session cleanly:

```python
from facts_tool import open_codebase
try:
    ft_codebase.close()
except NameError:
    pass
ft_codebase = open_codebase(
    facts_db="/absolute/path/to/facts.sqlite",
    project_db="/absolute/path/to/project.sqlite",
)
```

Define repeated investigations as typed functions, test them with
`call_function`, and publish them with `register_tool`:

```python
def facts_callers(symbol_ref: str, max_depth: int = 1) -> list[dict[str, object]]:
    """Return stored callers with their declaration evidence."""
    return [
        caller.to_dict()
        for caller in ft_codebase.get(symbol_ref).callers(max_depth=max_depth)
    ]
```

Use the registered tool for later questions. Close `ft_codebase` before
replacing databases, reloading the package, or ending the investigation.
