# Typed Python REST client

← [User guide index](../README.md)

The REST client connects to a running facts-tool server. The server owns database
and cache paths. The local `Facts` SDK remains available for direct SQLite access;
it does not require an HTTP server and retains lazy query iteration.

```sh
python -m pip install 'facts-tool[rest]'
```

Import `Client` or `AsyncClient` from `facts_tool.rest`. Both expose resource
collections backed by `/api/v2`; existing v1 convenience methods remain for
compatibility. Requests, returned resources, results and exceptions are typed.
No terminal output parsing or generic CLI argument construction is required.

## Register and update resources

```python
from facts_tool.rest import Client, NewClone, CompilationCommand

with Client("http://127.0.0.1:42817") as client:
    repo = client.repositories.create(
        name="example",
        clones=[NewClone(label="main", path="/workspace/example")],
    )
    repo = client.repositories.wait_until_ready(repo.id, timeout=120)
    print(repo.id, repo.active_clone_id)

    # Use a real clone ID returned by the repository.
    repo = client.repositories.update(
        repo.id,
        active_clone_id=repo.clones[0].id,
    )

    for source in client.files.list(repository="example"):
        print(source.id, source.path)
```

All filesystem paths are on the server host. Registering a repository schedules
automatic discovery/import/extraction when monitoring is enabled. It returns
before indexing finishes; watcher and index status expose progress.
`wait_until_ready()` waits for automatic import, extraction, and index publication.
It raises on processing failure or its optional timeout; a repository with no
compilation commands must be configured before it can become ready for analysis.

Repository updates own clone registrations and `active_clone_id`; there is no
separate clone endpoint. File updates own compiler settings:

```python
client.files.update(
    file_id,
    compilation_command=CompilationCommand(
        driver="clang++",
        working_directory="/workspace/example/build",
        arguments=["-std=c++23", "-I../include"],
    ),
)
```

Omitted update fields are unchanged. A supplied clone list replaces the
registered collection; a supplied compiler command replaces its old settings.
Removing registrations never deletes source directories or files.

## Lazy symbol lookup

```python
from facts_tool.rest import Client, SymbolKind

with Client("http://127.0.0.1:42817") as client:
    symbols = client.symbols.find(
        qualified_name="example::Widget",
        kind=SymbolKind.CLASS,
    )
    for symbol in symbols:
        print(symbol.symbol_id, symbol.qualified_name, symbol.definition)
```

Name lookup is a case-sensitive literal prefix search by default. `%` and `_`
in supplied names are literal characters. Set `match="exact"` for equality:

```python
symbols = client.symbols.find(
    qualified_name="example::Widget",
    match="exact",
).collect()
```

Collections fetch pages only as iteration advances. `.collect()` explicitly
loads all results into a list. Filters, resource IDs and pagination are encoded
as URL/query parameters; `GET` never sends a JSON body. Symbol cursors identify
an index revision. If it expires while iterating, handle the reported conflict
and restart the query; the client does not silently mix revisions.

Use `client.symbols.get(symbol_id)` for one resource. Repository and component
filters are optional; clients do not need catalog or facts-database filenames.
Missing names return an empty collection; ambiguous analysis identities are
reported instead of silently resolved to the first match.

## Typed extraction and matching jobs

```python
from facts_tool.rest import Client, FileSelection, FileReference

with Client("http://127.0.0.1:42817") as client:
    selection = FileSelection(files=[
        FileReference(path="src/Widget.cpp", repository="example"),
    ])
    job = client.extractions.create(selection=selection, force=False)
    result = job.wait()
    print(result.files_processed, result.symbols_written)
```

A submission returns after the server accepts it. Polling and `.wait()` return
scalar summaries and counts; large record collections are fetched separately
through lazy result iterators. `job.refresh()` reads current
state, `job.wait()` polls until a terminal state and returns the operation's
result model, and `job.cancel()` requests cancellation with HTTP `DELETE`.
Cancellation errors do not imply that the underlying analysis was stopped.

Retry a failed or cancelled job after fixing its inputs or compiler settings:

```python
failed = client.imports.get(failed_job_id)
retry = failed.retry()
result = retry.wait()
print(retry.id, retry.metadata.retry_of)

# Or resubmit directly through the matching analysis collection:
retry = client.extractions.retry(failed_extraction_job_id)
```

`retry()` creates a new typed handle from the server's retained request. The
original handle, job state and diagnostics remain unchanged. Every analysis
collection supports it. The server uses current settings and catalog data;
provide a normal `.create(...)` request to change analysis options. Only retained
failed or cancelled jobs can be retried. Non-retryable states return a conflict;
expired or different-family job IDs return not found. Retained jobs do not
survive a server restart. Async clients provide `await job.retry()` and
`await client.extractions.retry(job_id)` with the same behavior.

`ExtractionResult`, matcher results, dependency results, call-graph results and
variable-flow results are separate types rather than dictionaries. Collections
of result records can be iterated lazily through the analysis resource, such as
`client.matches.results(job.id)` or `client.extractions.diagnostics(job.id)`. Extraction
success includes publication of its global symbol index updates.

Selections also include `RepositorySelection`, `DirectorySelection`,
`ComponentSelection` and explicit `AllSelection`. `FileIdentity` selects a
returned file ID instead of a path.

```python
job = client.matches.create(
    selection=selection,
    expression='cxxMethodDecl(hasName("run")).bind("method")',
    traversal="IgnoreUnlessSpelledInSource",
    capture_source=True,
)
result = job.wait()
```

The full Clang matcher language and arbitrary binding names are supported.
`MatcherBindings` maps optional semantic roles to those names.

## Call graphs and local-variable tracking

```python
from facts_tool.rest import SymbolReference, VariableReference, DeclarationLocation

job = client.callgraphs.create(
    root=SymbolReference(qualified_name="example::Service::run"),
    direction="callees",
    max_depth=10,
)
graph = job.wait()
for node in client.callgraphs.nodes(job.id):
    print(node.qualified_name)

job = client.variable_flow.create(
    function=SymbolReference(qualified_name="example::Service::run"),
    variable=VariableReference(
        name="request",
        declaration=DeclarationLocation(
            path="src/Service.cpp", line=42, column=9,
        ),
    ),
    direction="forward",
    interprocedural=True,
    max_call_depth=10,
)
flow = job.wait()
for edge in client.variable_flow.edges(job.id):
    print(edge.kind)
```

`SymbolReference` accepts a qualified name, USR or returned symbol ID. Analysis
roots resolve exact symbols, independent of prefix search defaults. A declaration
location disambiguates shadowed local variables. Current variable tracking is
forward only; backward requests are rejected. Tracking crosses functions by
default. Set `interprocedural=False` to restrict it to the starting function and
report call-depth boundaries. Results expose limits and coverage diagnostics;
uncertain aliasing or unresolved calls must not be interpreted as complete flow.

## Asynchronous clients

```python
import asyncio
from facts_tool.rest import AsyncClient, FileReference, FileSelection

async def main():
    async with AsyncClient("http://127.0.0.1:42817") as client:
        async for symbol in client.symbols.find(qualified_name="example::"):
            print(symbol.qualified_name)
        job = await client.extractions.create(
            selection=FileSelection(files=[
                FileReference(path="src/Widget.cpp", repository="example"),
            ]),
        )
        result = await job.wait()
        print(result.files_processed)

asyncio.run(main())
```

`AsyncClient` awaits network operations and yields during polling. Collection
factories such as `.find()` and `.list()` return async iterables; use `async for`
or `await collection.collect()`. Resource creation, reads, updates, cancellation
and waiting are awaited. Pass `token=` when server authentication is enabled.

See [REST resources and jobs](../09-rest-api/02-requests-and-jobs.md) for HTTP
semantics and [OpenAPI contract](../09-rest-api/08-openapi-contract.md) for schemas,
regeneration and compatibility details.
