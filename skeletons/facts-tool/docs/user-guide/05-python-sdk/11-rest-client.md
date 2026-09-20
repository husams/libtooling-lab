# Python REST client

← [User guide index](../README.md) · [Table of contents](../toc.md)

`facts_tool.rest` provides `Client` for synchronous scripts and `AsyncClient`
for asynchronous applications and agents. Clients search symbols and submit
source analysis using typed inputs. The server owns the project database, facts
locations, stored compiler options and global index; clients never pass database
paths to these resource methods. The local SQLite API remains `open_codebase`.

## Install and connect

Use Python 3.12 or newer. From the **repository root**:

```bash
python3.12 -m venv .venv-api
.venv-api/bin/python -m pip install './skeletons/facts-tool/python[rest]'
```

The `rest` extra installs HTTPX; the base SDK remains dependency-free. Developers
can run `uv sync --locked --extra rest` from `python/`. Install the native server
separately using [REST installation](../09-rest-api/05-installation.md).

Use the server's actual startup address or saved port. Authentication is explicit:
pass `token=` when needed; the client does not read the environment automatically.

```python
import os
from facts_tool.rest import Client

with Client("http://127.0.0.1:42817",
            token=os.environ.get("FACTS_TOOL_API_TOKEN")) as api:
    print(api.health())
    print(api.index_status())
```

Context managers close HTTP resources. Outside a context manager, call
`client.close()` or `await client.aclose()`.

## Find a symbol

The fully qualified name is the only required input:

```python
with Client("http://127.0.0.1:42817") as api:
    page = api.find_symbols("example::Widget", kind="class")
    for symbol in page.items:
        print(symbol.qualified_name, symbol.kind, symbol.usr,
              symbol.file_id, symbol.path, symbol.repo)
```

Optional filters are `kind`, `usr`, `repo` and `component`. Names match exactly.
Results include symbols across repositories and source files; overloads remain
separate. Each `Symbol` carries `qualified_name`, `kind`, `usr`, `file_id`,
`path`, `repo`, `clone`, `component` and `is_definition`. The flag identifies
declaration-only fallbacks when a definition is unavailable. An absent name
returns an empty page.

`SymbolPage.items` is a tuple. Follow its opaque `next_cursor` with unchanged
filters and `limit` (default 50, maximum 500):

```python
with Client("http://127.0.0.1:42817") as api:
    cursor = None
    while True:
        page = api.find_symbols("example::render", kind="function",
                                limit=100, cursor=cursor)
        for symbol in page.items:
            print(symbol.usr, symbol.path)
        cursor = page.next_cursor
        if cursor is None:
            break
```

Index refresh can invalidate a cursor; restart from the first page when the
server rejects a stale cursor. Inspect `index_status()` before relying on a
freshly submitted analysis appearing in the results.

## Extract, match and analyze dependencies

Use `FileSelector(path, repo=None, clone=None, component=None)` to identify a
registered server-side source file. Absolute paths need no additional selectors
when unambiguous. Relative paths start at the selected clone's root, or at the
component root when supplied. An omitted clone uses the active clone. Clone
selectors accept a registered label, directory, or decimal ID string; `..` is rejected.

A registered header uses the same selector, such as
`FileSelector("include/widget.hpp", repo="core")`. When it has no stored compile
command, the server finds an including translation unit automatically. Equivalent
contexts are accepted; conflicting contexts fail the job with
`ambiguous_compilation_context`, and no registered includer fails with
`compilation_context_unavailable`. This applies to `extract()`, `match()` and
`dependencies()` without additional SDK arguments.

```python
from facts_tool.rest import Client, DomainJob, FileSelector

source = FileSelector("src/widget.cpp", repo="core", clone="main")

with Client("http://127.0.0.1:42817") as api:
    job = api.extract(source, force=False)
    finished = api.wait(job.id, timeout=1800).raise_for_status()
    assert isinstance(finished, DomainJob)
    print(finished.result)

    job = api.match(source, 'cxxRecordDecl(hasName("example::Widget"))')
    matched = api.wait(job.id, timeout=1800).raise_for_status()
    assert isinstance(matched, DomainJob)
    print(matched.result)

    job = api.dependencies(source)
    analyzed = api.wait(job.id, timeout=1800).raise_for_status()
    assert isinstance(analyzed, DomainJob)
    print(analyzed.result)
```

Submission returns a `DomainJob` after acceptance, before analysis begins.
Match accepts optional `traversal="AsIs"` (or `"IgnoreUnlessSpelledInSource"`),
`relation_kind=None`, and `capture_source=False`. Dependency analysis requires
only the selector. No method above accepts CLI tokens or database locations.

`match()` preserves facts outside the matched evidence. Use `extract()` after
symbol renames or deletions to refresh the selected translation unit's owned
declarations. Shared header facts remain when exclusive ownership is unknown;
historical local and parameter value identities remain for pointer-analysis evidence.
See [stored facts and completion](../09-rest-api/02-requests-and-jobs.md#extract-match-and-analyze-dependencies)
for the refresh semantics.

`DomainJob` contains `id`, `operation`, `state`, timestamps, a structured `result`
dictionary on success, and an `OperationError(code, message)` on failure. It
exposes `done`, `succeeded` and `raise_for_status()`. Successful results may
include `diagnostics`, with severity, message, file, line and column. Failed
operations expose these records through `job.error.details["diagnostics"]`.
Results and errors need no terminal-output parsing. `wait()` returns any
terminal state; call `raise_for_status()` when success is required. Retained job records can be evicted and disappear at restart.

## Async applications and index readiness

`AsyncClient` exposes the same methods with `await`. Network requests and job
polling yield to the event loop. The server also moves database and Clang work
off its HTTP event loop; concurrent submissions do not imply parallel extraction.

```python
import asyncio
import os
from facts_tool.rest import AsyncClient, FileSelector

async def wait_for_index(api):
    async with asyncio.timeout(120):
        while True:
            status = await api.index_status()
            if status.state == "failed":
                raise RuntimeError(status.error)
            if status.state == "ready" and not status.pending:
                return
            await asyncio.sleep(0.1)

async def main():
    async with AsyncClient(
        "http://127.0.0.1:42817", token=os.environ.get("FACTS_TOOL_API_TOKEN")
    ) as api:
        await wait_for_index(api)
        source = FileSelector("src/widget.cpp", repo="core")
        job = await api.extract(source)
        (await api.wait(job.id, timeout=1800)).raise_for_status()
        await wait_for_index(api)
        page = await api.find_symbols("example::Widget", kind="class")
        print(page.items)

asyncio.run(main())
```

The startup scan and post-analysis refresh are background tasks. A successful
job does not itself mean the global index has been published. `IndexStatus`
contains `state`, `pending`, `files`, `symbols`, `error` and `updated_at`. Queries
read the last completed index while a refresh runs. Before the first successful
scan they return `503 index_not_ready`. A failed refresh preserves the old index.

## Errors, deadlines and cancellation

| Exception | Meaning and useful attributes |
|---|---|
| `ApiError` | HTTP rejection; `status_code`, `message`, optional `code` and `details` |
| `TransportError` | Connection, network or HTTP request timeout failure |
| `ProtocolError` | Invalid or unexpected JSON response |
| `JobFailedError` | Completed job failed or was cancelled; `job`, `job_id` |
| `JobTimeoutError` | Client stopped polling before completion; `job_id`, `timeout` |

Bad selectors and analysis errors can appear in an accepted job's structured
`error`. Use `get_job(id)` for one snapshot or `list_jobs()` for retained metadata.
Job lists contain metadata with `result: None`; fetch an individual job for its
full result and diagnostic details. `cancel_job(id)` cancels queued native work; a running native analysis returns
`ApiError` with HTTP `409`, since it must finish without forced interruption.

`Client(..., timeout=10.0)` controls individual HTTP requests.
`wait(..., timeout=60)` controls the client's polling budget. The async client
bounds its wait with `asyncio.timeout`; the sync client checks elapsed time
between requests. A polling timeout, cancelled Python task or closed client
does not cancel the remote operation. Preserve the job ID and inspect it later.
The server's `serve --timeout` setting applies to legacy subprocess jobs, not
forced interruption of a running native resource operation.

Submissions are not automatically retried. After a network failure, work may
already have been accepted; inspect retained jobs before repeating it. Clients
do not follow redirects or use proxy environment settings.

## Server administration

| Method | Result |
|---|---|
| `health()` | HTTP health response dictionary |
| `index_status()` | Typed `IndexStatus` for the global symbol index |
| `openapi()` / `openapi_yaml()` | Live OpenAPI document as a dictionary or YAML string |
| `watch_status()` | Watcher activity and recent job IDs |
| `get_job(id)` | `DomainJob` or legacy `Job` snapshot |
| `list_jobs()` | Retained native and compatibility job metadata |
| `cancel_job(id)` | Snapshot after cancellation, or HTTP `409` for running native work |
| `shutdown()` | Orderly shutdown acknowledgement |

Await these methods on `AsyncClient`. Shutdown cancels queued work and waits
for active native work to complete. See
[Deployment and operation](../09-rest-api/06-deployment.md) for service management.

## Deprecated command compatibility

Existing `submit(*arguments)`, `command(path, *arguments)` and
`run(*arguments, timeout=...)` methods remain available for older clients. They
submit CLI tokens and return legacy `Job` records with captured `stdout`,
`stderr`, exit codes and output-limit flags. `run()` submits and waits, raising
for failure unless `check=False`; `commands()` discovers the legacy catalog.
These methods are deprecated compatibility wrappers. Use the resource methods
above for symbol lookup, extraction, matching and dependency analysis.

Endpoint methods are generated from the
[OpenAPI YAML contract](../09-rest-api/08-openapi-contract.md). Async generated
operations await HTTPX directly. See
[Requests and jobs](../09-rest-api/02-requests-and-jobs.md) for the HTTP schemas,
limits and index lifecycle.
