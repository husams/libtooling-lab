# Python REST client

← [User guide index](../README.md) · [Table of contents](../toc.md)

`facts_tool.rest` wraps the server's complete HTTP interface. Use `Client` in
synchronous scripts and `AsyncClient` in async applications and agents. Local
SQLite queries still use `open_codebase`; the REST clients send CLI commands
to the native server, which runs them with its own filesystem permissions.

## Install and connect

Install Python 3.12 or newer, then run this from the **repository root**:

```bash
python3.12 -m venv .venv-api
.venv-api/bin/python -m pip install './skeletons/facts-tool/python[rest]'
```

The `rest` extra installs HTTPX; the base SDK remains dependency-free. For a
development environment use `uv sync --locked --extra rest` from the SDK's
`python/` directory. Install the native server separately using
[REST installation](../09-rest-api/05-installation.md).

Start the server, then use the actual address printed at startup or saved in
its server YAML. Port `42817` below is an example. Authentication is explicit:
pass the server's token through `token=`; the client does not read the environment
for you.

```python
import os

from facts_tool.rest import Client

with Client("http://127.0.0.1:42817",
            token=os.environ.get("FACTS_TOOL_API_TOKEN")) as api:
    print(api.health())
    print(api.commands())
    result = api.run("config", "show", timeout=30)
    print(result.stdout)
```

The context manager closes HTTP resources. Outside a context manager, call
`client.close()` or `await client.aclose()` for the async client.

## Submit commands and wait for results

Pass individual CLI tokens, without the executable name or shell quoting.
Paths refer to the **server's filesystem**. The following synchronous examples
reuse the `os` and `Client` imports above. Submit an import, wait for it to
succeed, and then extract facts:

```python
with Client("http://127.0.0.1:42817",
            token=os.environ.get("FACTS_TOOL_API_TOKEN")) as api:
    imported = api.run("import", "-p", "/workspace/project/build", timeout=300)
    extracted = api.run("extract", "-o", "/workspace/facts.db", timeout=1800)
    print(extracted.stdout)
```

`run()` submits a job, polls it, and raises `JobFailedError` if its final state
is failed or cancelled. Set `check=False` to inspect that result yourself.
`timeout` here is the client's polling budget; the server's separate
`serve --timeout` setting controls worker execution time.

Use `submit()` when other work should happen before polling. `command()` uses
a named command endpoint; nested CLI names are separated by slashes:

```python
with Client("http://127.0.0.1:42817",
            token=os.environ.get("FACTS_TOOL_API_TOKEN")) as api:
    job = api.submit("symbol", "list", "-f", "/workspace/facts.db")
    print(job.id, job.state)
    finished = api.wait(job.id, timeout=60, poll_interval=0.2)
    finished.raise_for_status()
    print(finished.stdout)

    help_job = api.command("analyse/call-graph", "--help")
    print(api.wait(help_job.id).raise_for_status().stdout)
```

`wait()` returns any terminal state; call `raise_for_status()` when success is
required. `get_job(id)` retrieves one snapshot and `list_jobs()` returns retained
job metadata. `cancel_job(id)` requests cancellation; poll afterwards to observe
its final state. A job can finish before the cancellation request arrives.

## Async applications

`AsyncClient` exposes the same operations with `await` and async context
management. Network requests and polling yield to the event loop:

```python
import asyncio
import os

from facts_tool.rest import AsyncClient


async def main():
    async with AsyncClient(
        "http://127.0.0.1:42817", token=os.environ.get("FACTS_TOOL_API_TOKEN")
    ) as api:
        print(await api.health())
        job = await api.submit("config", "show")
        result = await api.wait(job.id, timeout=30)
        result.raise_for_status()
        print(result.stdout)


asyncio.run(main())
```

The HTTP server remains responsive while work runs, but it executes one CLI job
at a time. Concurrent client requests do not make extraction jobs run in parallel.

## Job results and output

`Job` is an immutable value object:

| Fields | Meaning |
|---|---|
| `id`, `arguments` | Server-assigned string ID and CLI tokens as a tuple |
| `state` | `queued`, `running`, `succeeded`, `failed`, or `cancelled` |
| `done`, `succeeded` | Convenience properties for completion and success |
| `exit_code` | CLI exit status, or `None` before completion |
| `stdout`, `stderr` | Captured strings; `None` when omitted from job-list metadata |
| `truncated`, `timed_out` | Server output cap or worker deadline was reached |
| `created_at`, `started_at`, `finished_at` | Unix epoch milliseconds; start/finish may be `None` |

Output becomes available when the command completes. Commands that support
`--format json` still return that JSON as a `stdout` string; parse it with
`json.loads(result.stdout)` after checking success and `truncated`. The server
caps each output stream at 4 MiB. For large local database queries, use the
[lazy SQLite query API](10-query-performance.md). Job records are held in memory
and can be evicted or lost on server restart.

## Errors, deadlines and cancellation

Import exceptions from `facts_tool.rest`:

| Exception | Meaning and useful attributes |
|---|---|
| `ApiError` | HTTP rejection, such as authentication, missing job or full queue; `status_code`, `message` |
| `TransportError` | HTTP connection, network or request timeout failure |
| `ProtocolError` | Invalid or unexpected JSON response |
| `JobFailedError` | CLI job failed or was cancelled; `job`, `job_id` |
| `JobTimeoutError` | Client stopped waiting before completion; `job_id`, `timeout` |

`Client(..., timeout=10.0)` sets the HTTP request timeout. `wait(..., timeout=60)`
and `run(..., timeout=60)` set a polling budget. The sync client checks elapsed
time between requests and caps HTTPX inactivity timeouts to the remaining budget;
a response that keeps delivering data can exceed that wall-clock budget. The
async client bounds the whole wait with `asyncio.timeout`. A polling timeout, a
cancelled Python task, or closing a client **does not cancel the remote job**.
Retain its ID and request cancellation explicitly when needed:

```python
from facts_tool.rest import JobTimeoutError

with Client("http://127.0.0.1:42817",
            token=os.environ.get("FACTS_TOOL_API_TOKEN")) as api:
    job = api.submit("extract", "-o", "/workspace/facts.db")
    try:
        result = api.wait(job.id, timeout=60)
        result.raise_for_status()
    except JobTimeoutError:
        api.cancel_job(job.id)
        print(api.wait(job.id, timeout=30).state)
```

Requests are not automatically retried. After a network failure during submission,
the command may already have been accepted; inspect retained jobs before repeating
work. The client does not follow redirects or use proxy environment settings.

## Server administration

| Method | Result |
|---|---|
| `health()` | Health response dictionary |
| `commands()` | List of command dictionaries with `path` and `endpoint` |
| `openapi()` | OpenAPI document dictionary |
| `openapi_yaml()` | The same OpenAPI contract as a YAML string |
| `watch_status()` | Watcher state and recent job IDs |
| `get_job(id)` | One `Job` including available captured output |
| `list_jobs()` | List of retained `Job` metadata snapshots |
| `cancel_job(id)` | Current `Job` after requesting cancellation |
| `shutdown()` | Shutdown acknowledgement dictionary |

Await these methods on `AsyncClient`. `shutdown()` stops the server and cancels
queued or active work. See [Deployment and operation](../09-rest-api/06-deployment.md)
for service management and [Requests and jobs](../09-rest-api/02-requests-and-jobs.md)
for HTTP limits and status codes.

Endpoint operations are generated from the
[OpenAPI YAML contract](../09-rest-api/08-openapi-contract.md). Async operations
await HTTPX directly, and both clients retain their existing polling helpers.
