# Typed REST API v2

Install `facts-tool-query[rest]`. `Client` and `AsyncClient` provide typed resource
namespaces; the local database SDK still works without HTTPX or a server.

```python
from facts_tool.rest import Client, NewClone

with Client("http://localhost:8080") as api:
    repo = api.repositories.create(
        name="example", clones=[NewClone(path="/workspace/example", label="main")]
    )
    repo = api.repositories.wait_until_ready(repo.id, timeout=300)
    api.repositories.update(repo.id, active_clone_id=repo.clones[0].id)
```

Repository registration triggers server discovery, import, extraction, and index
publication when monitoring is enabled. `wait_until_ready` verifies the active
clone is in the watcher snapshot, its processing cycle completed, all registered
sources are indexed, and the global index is published. It raises
`RepositoryNotReady` if monitoring is disabled or compilation configuration is
missing. Its timeout leaves server processing running. Filesystem paths are paths
on the server host; clients never supply project or facts database paths. Keep the client context open while running the examples below.

```python
symbols = api.symbols.find(qualified_name="example::Widget")
for symbol in symbols:
    print(symbol.symbol_id, symbol.qualified_name)

exact = api.symbols.find(
    qualified_name="example::Widget", match="exact"
).collect()
```

Searches default to literal, case-sensitive prefixes. Collection methods issue no
HTTP request until iteration. They request bounded pages using GET query parameters;
`.collect()` explicitly materializes all records. IDs are opaque strings.

```python
from facts_tool.rest import FileReference, FileSelection

job = api.extractions.create(
    selection=FileSelection([FileReference("src/Widget.cpp", repository="example")])
)
summary = job.wait(timeout=120)
print(summary.symbols_written, summary.index_revision)
for file in api.extractions.results(job.id):
    print(file.path, file.symbol_count)
```

Job snapshots contain scalar summaries. Large record collections in result models
are `None` until explicitly fetched; `None` does not mean an empty result. Use
`.results(job.id)` or the typed `.nodes`, `.edges`, `.paths`, `.boundaries`, and
`.diagnostics` accessors. `.refresh()` updates a snapshot; `.cancel()` uses DELETE.
A local wait timeout or cancelled coroutine does not cancel the server job.

```python
from facts_tool.rest import SymbolReference, VariableReference

graph = api.callgraphs.create(root=SymbolReference(qualified_name="Service::run"))
graph.wait()
for edge in api.callgraphs.edges(graph.id):
    print(edge.source, edge.target)

flow = api.variable_flow.create(
    function=SymbolReference(qualified_name="Service::run"),
    variable=VariableReference(name="request"), interprocedural=True,
)
flow.wait()
for boundary in api.variable_flow.boundaries(flow.id):
    print(boundary.reason, boundary.detail)
```

`AsyncClient` uses the same models and names. Await create, get, update, wait,
cancel, and `.collect()`. Use `async for` on its collection methods; `.find`,
`.list`, and `.results` themselves are lazy factories and are not awaited.

`repositories.update` sends PATCH; supplied clone arrays replace that collection.
Omitted update fields are untouched; `remote_url=None` explicitly clears it.
`files.update(id, compilation_command=...)` updates compilation settings on the
file. `watcher.update_settings` sends PATCH and `replace_settings` sends PUT.

Existing flat v1 methods remain compatible. `api.legacy`, `LegacyClient`, and
`LegacyAsyncClient` provide explicit access to v1; `api.dependencies(file)` remains
legacy while `api.dependencies.create(selection=...)` uses v2.
