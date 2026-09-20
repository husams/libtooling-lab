# OpenAPI contract and generated code

← [User guide index](../README.md) · [Table of contents](../toc.md)

The REST interface is defined in OpenAPI 3.1 YAML. Its source is
[`src/apis/openapi/openapi.yaml`](../../../src/apis/openapi/openapi.yaml),
with small referenced files for operations, request bodies and response schemas.
Edit this contract when changing the HTTP interface, then regenerate its bindings.

## Read the running server's contract

Both formats describe the same interface, including the current CLI command
catalog and authentication requirements:

```bash
API=http://127.0.0.1:42817
curl --fail --silent --show-error "$API/openapi.yaml" -o facts-tool-openapi.yaml
curl --fail --silent --show-error "$API/openapi.json" -o facts-tool-openapi.json
```

Use the server's actual allocated port. Add
`-H "Authorization: Bearer $FACTS_TOOL_API_TOKEN"` when authentication is enabled.
Import the downloaded YAML into Swagger Editor or other OpenAPI tooling to
inspect requests, responses and examples. The server rejects browser-origin
requests; use command-line or Python clients to call it.

The contract includes `/v1/jobs`, job polling and cancellation, health, command
discovery, watcher status, shutdown and the two contract endpoints. The generic
`/v1/commands/{commandPath}` operation accepts nested CLI names such as
`repo/add`; encode its slash as `repo%2Fadd` when substituting the path parameter.
Existing `/v1/commands/repo/add` requests also work. The live document adds
concrete endpoints for all registered CLI commands and aliases automatically.

## Regenerate and check

From `skeletons/facts-tool`, install the development tools and generate:

```bash
python3.12 -m venv .venv-openapi
.venv-openapi/bin/python -m pip install -r tests/e2e/requirements.txt
.venv-openapi/bin/python scripts/generate_openapi.py
.venv-openapi/bin/python scripts/generate_openapi.py --check
```

The generator validates the contract and resolves its local references. It emits
native route definitions, request limits and the embedded specification under
`src/apis/generated`, plus the Python `rest/client.py`, `rest/async_client.py`
and endpoint metadata under `python/src/facts_tool/rest/generated`.
Commit the YAML and generated files
together. `--check` reports stale generated files without rewriting them.

Native operation handlers and job execution remain small, handwritten functions.
The generated route table selects those handlers; generated limits are used in
HTTP parsing and argument validation. Python clients use generated operation
bindings while retaining lifecycle and polling code from small source templates.
Generated code is checked in, so a normal native build or installed Python
client does not run the generator or require its development dependencies.
Operation IDs connect the contract to native handlers and SDK adapters. Adding
an operation also requires its handler and adapter; unsupported request shapes
are rejected during generation before existing bindings are rewritten.

## Asynchronous execution

Submitting a CLI command returns HTTP `202` with a job ID and a `Location`
polling URL. Awaiting that submission waits only for acceptance. Poll the job
until it reaches `succeeded`, `failed` or `cancelled`, then inspect its exit code
and captured output. See [Requests and jobs](02-requests-and-jobs.md).

The native server uses asynchronous socket, pipe, timer and inotify operations.
CLI commands run in child processes; repository scanning runs on a worker.
The OpenAPI documents are prepared at startup and served from memory.
One queue serializes CLI jobs while HTTP requests remain responsive.

The generated `AsyncClient` operations await HTTPX asynchronous requests.
Polling yields to the Python event loop:

```python
import asyncio
from facts_tool.rest import AsyncClient

async def main():
    async with AsyncClient("http://127.0.0.1:42817") as api:
        document = await api.openapi_yaml()
        job = await api.command("repo/list")
        result = await api.wait(job.id, timeout=30)
        print(result.raise_for_status().stdout)

asyncio.run(main())
```

Pass `token=` when required. `Client` provides the same contract operations for
synchronous callers; see [Python REST client](../05-python-sdk/11-rest-client.md).
Contract validation, generation checks and real-server BDD scenarios are covered
in [Architecture and testing](04-architecture-and-testing.md).
