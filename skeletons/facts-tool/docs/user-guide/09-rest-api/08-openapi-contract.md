# OpenAPI contract and generated code

← [User guide index](../README.md)

The authoritative contract is OpenAPI 3.1:
[`src/apis/openapi/openapi.yaml`](../../../src/apis/openapi/openapi.yaml).
Referenced YAML files under `v2/paths` and `v2/schemas` define `/api/v2` resources,
requests, operation-specific job results, pagination and errors. Existing `/v1`
paths and schemas remain for compatibility.

## Read the running server's contract

```sh
API=http://127.0.0.1:42817
curl --fail --silent --show-error "$API/openapi.yaml" -o facts-tool-openapi.yaml
curl --fail --silent --show-error "$API/openapi.json" -o facts-tool-openapi.json
```

Use the actual listening address. Add an Authorization header when a token is
configured. Both downloads are self-contained and describe the same API. The
served document reflects whether Bearer authentication is enabled. Import it
into OpenAPI tooling to inspect typed requests and responses. The server rejects
browser-origin HTTP calls; native and Python clients can call it directly.

## Resource contract

API versions and addressed resource IDs are in URL paths. `GET` reads resources
and uses query parameters for search, filtering and pagination. `POST` registers
resources or creates jobs. `PATCH` changes selected fields, `PUT` replaces
complete watcher settings, and `DELETE` unregisters resources or cancels jobs.
There are no v2 CLI-token arrays, stdout result strings, or generic command
submission endpoints. Compiler argument arrays are typed file configuration.

Repository clone registrations and `active_clone_id` are fields of the
repository resource. File compilation settings are fields of the file resource.
Database filenames are server-owned and absent from creation/analysis requests.
Unknown request fields are rejected. Selections are discriminated unions for
files, directory, component, repository and explicit all-source selection.
Import and scan accept only their supported root-selection variants.

Extraction, matching, import, dependencies, call graphs, local-variable flow,
scanning and index rebuilds each have their own request, job and result schemas.
A completed extraction returns a `V2ExtractionResult` summary, never an
unrestricted JSON object. Large record arrays are optional in result schemas
and are read through paginated result collections; ordinary polling and `.wait()`
only fetch scalar summaries and counts. AST bindings form a named map of typed node records; arbitrary binding
names remain valid. Graph and flow nodes, edges, boundaries and diagnostics each
have defined properties. Result page alternatives are typed according to their
`collection` query parameter; `anyOf` permits an empty page without falsely
claiming that it belongs to only one node/edge alternative.

Symbol lookup defaults to case-sensitive literal prefix matching. `match=exact`
selects equality. Analysis root identities remain exact. Collection pages expose
`items` and `next_cursor`; symbol pages also identify the stable index revision.
All current collection limits default to 50 and have maximum 500.

See [REST resources and jobs](02-requests-and-jobs.md) for HTTP examples and
[Python REST client](../05-python-sdk/11-rest-client.md) for resource objects,
typed jobs, lazy iteration and asynchronous use.

## Regenerate and check

From `skeletons/facts-tool`:

```sh
python3.12 -m venv .venv-openapi
.venv-openapi/bin/python -m pip install -r tests/e2e/requirements.txt
.venv-openapi/bin/python scripts/generate_openapi.py
.venv-openapi/bin/python scripts/generate_openapi.py --check
```

The generator resolves local references and validates the complete document.
It writes native route metadata, request limits and the embedded specification
under `src/apis/generated`, and the existing v1 Python client adapters and
metadata under `python/src/facts_tool/rest/generated`. The v2 Python resource
models and adapters are maintained alongside that generated compatibility layer.
They are checked against the v2 contract; they are not produced by an external
OpenAPI client generator. Native analysis handlers are shared application
services, not generated CLI wrappers.

The embedded specification is split into small include files to preserve the
repository's generated-file line limit. `--check` reports drift without changing
files. Commit YAML and generated outputs together. Checked-in output means
normal native builds and installed clients do not require YAML or generation
dependencies. `--bundle path.yaml` also exports a single portable document.

Contract validation includes method semantics and route identities. Standard
OpenAPI tools can read the split source with relative-reference support, or the
bundled document served by the application.

## Execution, cancellation and index visibility

Job submission returns `202 Accepted` and a `Location` header before analysis
finishes. Read that resource until `succeeded`, `failed` or `cancelled`.
Cancellation uses `DELETE`: queued jobs can cancel immediately, running work
enters `cancelling` and stops at a safe checkpoint. Repeated cancellation is
idempotent and terminal outcomes are retained.

Results are operation-specific even though lifecycle fields are shared.
Extraction and matching publish their updated facts to the global index before
their v2 jobs report success. During refresh, queries use the last published
index. Before initial index availability they return a readiness error.

HTTP sockets, monitoring and polling remain asynchronous. Native work runs on
workers and mutating work is serialized. Deprecated v1 compatibility operations and existing internal automatic watcher
batches use the command worker. Public v2 handlers call native application
services directly. Job snapshots are retained in memory and can be
evicted or lost on restart; project facts and the global index persist.
