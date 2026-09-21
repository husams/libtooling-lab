# Use the REST wrapper in IPython-MCP

When IPython-MCP is available, follow its installed runtime skill. Check
`runtime_status`, then use runtime `search`/`inspect` to reuse an existing
client. Execute Python in that runtime; do not create a separate shell Python
session to stand in for it.

## Install in the actual runtime

Install a compatible wheel with the REST extra, or the distribution:

```python
%pip install "facts-tool-query[rest]"
```

For authorized development, an editable installation of the repository's
`skeletons/facts-tool/python[rest]` path is also valid. Check installed
capabilities rather than using checkout documentation as proof.
Use the project's [development workflow](../../../../python/docs/development.md)
when building an artifact. Report import/version failures accurately.

## Keep a client open

Use the configured listener URL and optional token. Paths in requests belong
to the server; the IPython runtime need not mount its databases or sources.

```python
from facts_tool.rest import Client

try:
    ft_client.close()
except NameError:
    pass
ft_client = Client(base_url, token=api_token)
print(ft_client.server.readiness())
```

Close the old client before replacing it, reloading the package, or ending the
session. Consume lazy collections before closing the client.

Define reusable investigations as typed functions and, when appropriate, test
with `call_function` and publish with `register_tool`:

```python
from dataclasses import asdict
from itertools import islice
from facts_tool.rest import SymbolReference

def facts_callers(symbol_id: str, max_depth: int = 1) -> dict[str, object]:
    """Return a bounded preview with job identity and coverage."""
    job = ft_client.callgraphs.create(
        root=SymbolReference(symbol_id=symbol_id),
        direction="callers",
        max_depth=max_depth,
    )
    summary = job.wait(timeout=120)
    preview = list(islice(ft_client.callgraphs.edges(job.id), 20))
    return {
        "job_id": job.id,
        "summary": asdict(summary),
        "edges": [asdict(edge) for edge in preview],
        "preview_limited": summary.edge_count > len(preview),
    }
```

This preview is intentionally bounded; read frontier/diagnostic collections
before claiming complete behavior. Reuse an existing suitable retained job
when possible. Do not register a tool that hides truncation, boundaries, or
ambiguity. See [agent workflows](agent-workflows.md) for `AsyncClient`;
in an async runtime, await network operations and use `async for`.
