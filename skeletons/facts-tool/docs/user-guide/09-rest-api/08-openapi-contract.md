# OpenAPI contract and generated code

← [User guide index](../README.md) · [Table of contents](../toc.md)

The REST interface is defined in OpenAPI 3.1 YAML. Its source is
[`src/apis/openapi/openapi.yaml`](../../../src/apis/openapi/openapi.yaml),
with small referenced files for operations, request bodies and response schemas.
Edit this contract when changing the HTTP interface, then regenerate its bindings.

## Read the running server's contract

Both formats describe the same typed symbol, file-analysis and job interface,
including authentication requirements and deprecated command compatibility:

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

The resource contract includes symbol lookup (`GET /v1/symbols`), extraction,
matching, dependency analysis and global index status. Analysis request schemas
use a typed `file` selector; match also requires a Clang DSL `query`. Additional
properties are rejected. Database paths and CLI arguments are absent from these
schemas. Jobs return structured results and errors.

The contract also defines polling, cancellation, health, watcher status and
shutdown. `/v1/commands/{commandPath}` and `POST /v1/jobs` are marked deprecated
for existing command clients. Their live CLI catalog remains discoverable for
compatibility; it does not define the resource API.

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

Submitting extraction, matching or dependency analysis returns HTTP `202` with
a job ID and a `Location` polling URL before the operation starts. Awaiting the
submission waits for acceptance. Poll until `succeeded`, `failed` or `cancelled`,
then inspect the structured `result` or `error`. Index publication follows
successful extraction/matching asynchronously; inspect `GET /v1/index` before
querying newly indexed symbols. See [Requests and jobs](02-requests-and-jobs.md).

The server uses asynchronous sockets, timers and inotify. Typed handlers call
shared native services on workers, with server-side identity and storage
resolution. Symbol database queries also run off the HTTP event loop. Only the
deprecated command compatibility API invokes CLI child processes. OpenAPI
documents are prepared at startup and served from memory.

The generated `AsyncClient` operations await HTTPX asynchronous requests.
Polling yields to the Python event loop:

```python
import asyncio
from facts_tool.rest import AsyncClient

async def main():
    async with AsyncClient("http://127.0.0.1:42817") as api:
        document = await api.openapi_yaml()
        print(document)
        status = await api.index_status()
        if status.state == "ready" and not status.pending:
            page = await api.find_symbols("example::Widget", kind="class")
            print(page.items)

asyncio.run(main())
```

Pass `token=` when required. `Client` provides the same contract operations for
synchronous callers; see [Python REST client](../05-python-sdk/11-rest-client.md).
Contract validation, generation checks and real-server BDD scenarios are covered
in [Architecture and testing](04-architecture-and-testing.md).
