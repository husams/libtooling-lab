# Server-backed agent workflows

Contents: [connection](#connect-and-check-readiness),
[jobs and results](#submit-targeted-work-and-consume-results),
[failures](#failures-and-evidence-limits), [async](#asynchronous-execution).

## Connect and check readiness

Install the distribution `facts-tool-query[rest]` in the execution environment
(Python 3.12 or later). Import from `facts_tool.rest`, not a legacy client.
Reuse the configured listener URL and credentials; the URL below is an example,
not a fixed port. Use the actual bound port if the server started with port zero.

```python
from facts_tool.rest import Client

with Client("http://127.0.0.1:42817", token=None) as client:
    print(client.server.health().status)
    print(client.server.readiness())
    print(client.index.status())
```

Keep the client open while creating jobs, polling, and consuming collections.
Supply `token=` from the existing credential configuration when authentication
is enabled; never print it. Inspect the installed wrapper and the served OpenAPI
contract if capabilities differ from checkout documentation.

Reuse catalog resources via `client.repositories.list()`. After registration,
clone switching, or awaiting automatic processing, use
`client.repositories.wait_until_ready(repo.id, timeout=120)`. This checks
watcher processing, compilation configuration, indexed source counts, and index
publication. It can raise `RepositoryNotReady` for disabled monitoring, missing
configuration, or processing failure. A `RepositoryTimeout` only stops waiting.
Use [manual import/extraction](rest-api.md) if monitoring is intentionally disabled.

The global index covers registered facts across repositories. Startup indexing,
watcher activity, and a ready listener are different states. Inspect watcher
errors/exclusions and the index state before interpreting an empty lookup.
A completed scan or import alone does not prove facts have been extracted.

## Submit targeted work and consume results

```python
from facts_tool.rest import FileReference, FileSelection

# Continue inside the open client context.
selection = FileSelection(files=[
    FileReference(path="src/Widget.cpp", repository="example"),
])
job = client.extractions.create(selection=selection, force=False)
summary = job.wait(timeout=120)
print(job.id, summary.coverage, summary.files_processed, summary.files_skipped)
for diagnostic in client.extractions.diagnostics(job.id):
    print(diagnostic.severity, diagnostic.message)
for result in client.extractions.results(job.id):
    print(result.path, result.symbol_count)
```

Use real registered files and their compiler commands. `force=False` permits
freshness-based skips; force reprocessing only when needed. Successful extraction
includes global-index publication. Re-query after refresh and retain remaining
coverage gaps. See [resource APIs](rest-api.md) for other selections and jobs.

`create()` accepts work and returns a handle; `refresh()` reads current state,
`wait()` returns its typed summary after success, and `cancel()` requests
cancellation. Retrieve an existing handle with the correct resource's
`get(job_id)`. IDs are opaque strings; do not hard-code a numeric run ID or
reuse an ID across analysis families.

A summary's `matches`, `nodes`, `edges`, or `diagnostics` can be `None`
because records were not fetched. Fetch them using the resource's separate
iterators. Collections issue requests as iteration advances; `limit` is a
**page size**, not an overall result cap. Use `itertools.islice` for a bounded
preview and disclose omitted output. Call `.collect()` only when a full
in-memory list is wanted. Page size defaults to 50; use at most 500.

## Failures and evidence limits

Handle `ApiError` (including typed validation, ambiguity, and conflict errors),
`TransportError`, `ProtocolError`, `JobFailed`, and `JobTimeoutError` at
their respective boundaries. Report the job ID and structured error code/message
when available; never read an older job as though it were the failed operation.
A local timeout or cancelled coroutine leaves server work running. Cancel
explicitly through `job.cancel()` when requested, then inspect the returned
state; a cancellation conflict does not mean analysis stopped.

A 503 `index_not_ready` means wait for index readiness, not zero symbols.
An expired symbol cursor produces a conflict: restart that query from its first
page and discard the earlier partial collection rather than mixing revisions.
Do not blindly resubmit a mutating request after an uncertain transport failure;
inspect retained jobs/resources first.

Check each operation's actual coverage fields, diagnostic collections, and
truncation/boundary records. Matcher completion covers only the selected TUs;
graph completion is not proof of all source behavior. Preserve nullable
coordinates and unavailable-location reasons. Job history is bounded and does
not survive server restart; facts/indexes persist. Record needed evidence
before retention expires.

## Asynchronous execution

```python
from facts_tool.rest import AsyncClient, SymbolReference

async def inspect_callers(base_url: str):
    async with AsyncClient(base_url) as client:
        async for symbol in client.symbols.find(qualified_name="example::"):
            print(symbol.symbol_id, symbol.qualified_name)
        job = await client.callgraphs.create(
            root=SymbolReference(qualified_name="example::Service::run"),
            direction="callers",
        )
        summary = await job.wait(timeout=120)
        print(summary.coverage, summary.truncated)
        async for edge in client.callgraphs.edges(job.id):
            print(edge.source, edge.target)
```

Await create/get/update/delete, readiness waits, refresh/cancel/wait, and
`.collect()`. Collection factories (`find`, `list`, `results`, `nodes`,
`edges`) return async iterables without being awaited. Use `async for`.
