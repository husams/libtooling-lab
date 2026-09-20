# REST requests and asynchronous jobs

← [User guide index](../README.md) · [Table of contents](../toc.md)

The server owns its project database, facts databases and global symbol index.
Clients identify symbols or source files; they do not supply database paths or
CLI argument arrays to the resource endpoints. Examples use the address saved
by `serve`:

```sh
API=http://127.0.0.1:42817
```

Add `-H "Authorization: Bearer $FACTS_TOOL_API_TOKEN"` when configured.

## Find a symbol across repositories

Only the exact fully qualified name is required. Kind, USR, repository and
component are optional filters:

```sh
curl -sS -G "$API/v1/symbols" \
  --data-urlencode 'qualified_name=example::Widget' \
  --data-urlencode 'kind=class'
```

A successful response contains structured records, rather than command output:

```json
{
  "items": [
    {
      "qualified_name": "example::Widget",
      "kind": "class",
      "usr": "c:@N@example@S@Widget",
      "file_id": 42,
      "is_definition": true,
      "path": "/workspace/core/include/widget.hpp",
      "repo": "core",
      "clone": "main",
      "component": "core"
    }
  ],
  "next_cursor": null
}
```

`is_definition` distinguishes a known definition from a declaration-only
fallback when no definition is available. Missing repository or clone identities are
reported as empty strings; an unlabeled known clone uses its numeric ID. Overloads and definitions in different files remain
separate records. Use `usr`
when a name has several overloads. `limit` defaults to 50 and accepts 1–500.
Follow `next_cursor` with the same filters and limit until it is `null`. A missing
symbol returns an empty `items` array. Do not infer that a symbol is missing
before the first index scan has completed.

## Identify a source file

Extraction, matching and dependency analysis use a `file` object:

```json
{"file":{"path":"/workspace/core/src/widget.cpp"}}
```

A relative path can specify its registered repository and clone:

```json
{"file":{"path":"src/widget.cpp","repo":"core","clone":"main"}}
```

`repo`, `clone` and `component` are optional when the selection is unambiguous.
Relative paths start at the clone root; when `component` is supplied, they start
at that component's root. An omitted clone uses the repository's active clone.
A clone selector accepts its registered label, directory, or decimal ID string. An absolute path
can identify a registered file without any other selectors. Explicit selectors
must agree with that path. Parent traversal (`..`), unregistered paths and
ambiguous file identities are rejected. The server uses the registered compile
options and resolves the corresponding databases internally.

The same selector works for a registered header:

```json
{"file":{"path":"include/widget.hpp","repo":"core"}}
```

If the header has no stored compile command, the server automatically finds a
registered translation unit that includes it and uses that compilation context.
Several including units are accepted when their compilation settings are
equivalent. Conflicting settings produce `ambiguous_compilation_context` (409);
a header with no registered includer produces `compilation_context_unavailable`
(422). These checks run in the background for extraction, matching and dependency
analysis: submission returns `202`, then a failed job carries the error code and
message. Clients do not select compiler arguments or supply a translation unit.

## Extract, match and analyze dependencies

Each operation returns HTTP `202` with a job ID and a `Location` header pointing
to `/v1/jobs/{id}`. Acceptance comes before the analysis starts.

```sh
curl -sS -X POST "$API/v1/extractions" \
  -H 'Content-Type: application/json' \
  -d '{"file":{"path":"src/widget.cpp","repo":"core"}}'

curl -sS -X POST "$API/v1/matches" \
  -H 'Content-Type: application/json' \
  -d '{"file":{"path":"src/widget.cpp","repo":"core"},"query":"cxxRecordDecl(hasName(\"example::Widget\"))"}'

curl -sS -X POST "$API/v1/dependencies" \
  -H 'Content-Type: application/json' \
  -d '{"file":{"path":"src/widget.cpp","repo":"core"}}'
```

Extraction additionally accepts `force: true`. Match requires a Clang AST matcher
DSL `query`; it optionally accepts `traversal` (`AsIs` or
`IgnoreUnlessSpelledInSource`), `relation_kind` and `capture_source`. Dependency
analysis needs only the file selection. See the
[OpenAPI contract](08-openapi-contract.md) for complete request schemas.

Poll the accepted job:

```sh
curl -sS "$API/v1/jobs/d1"
```

A resource job reports `operation`, `state`, timestamps, a structured `result`
on success and an `error` with `code` and `message` on failure. Compiler diagnostics are structured records with severity, message, file, line
and column, in `result.diagnostics` or `error.details.diagnostics`. Clients do
not parse terminal output. States are `queued`, `running`, `succeeded`, `failed` and
`cancelled`. Use the ID returned by submission; native IDs such as `d1` and compatibility
IDs share the polling endpoints.

| Operation result | Additional structured fields |
|---|---|
| Extraction | `symbol_count` in the resulting facts store |
| Match | `matches` records and `match_count` |
| Dependencies | `edges` with source/destination file IDs and paths, plus `edge_count` |

Each result also identifies the selected `file`, `operation`, and completion
status. Completion means the requested operation finished. Database filenames
are absent from these results.

Matching stores matched evidence while preserving other stored facts. Extraction
transactionally refreshes declarations owned by the selected translation unit and
removes disappeared declarations such as functions, types and global variables.
Use extraction after symbol renames or
deletions: a match does not replace all facts for a file. Shared header facts
are retained when the current ownership model cannot establish that they belong
exclusively to that translation unit. Historical local and parameter value
identities are also retained for pointer-analysis evidence, because their
offset-based USRs can change when a function body changes; their recorded source
positions can refer to an earlier version. The global index reflects these stored facts.

## Global index readiness and refresh

```sh
curl -sS "$API/health"
curl -sS "$API/v1/index"
```

Health confirms that HTTP is available. Index status separately reports
`state` (`queued`, `running`, `ready` or `failed`), `pending`, facts-file and
symbol counts, `error`, and the last successful `updated_at` time in Unix epoch
milliseconds.

At startup the server schedules a background scan of known existing facts
files. It stores fully qualified name, kind, USR and defining file ID in
`project.db`'s `global_symbol_index`. Known files include registered facts
locations and destinations derived from the configured facts template. New
files without a template use `facts/<file-id>.db` beside the project database.
Clients never need these storage paths.

Successful extraction and matching schedule another background index refresh.
Job completion and index publication are separate events: wait until index
status is `ready` with `pending: false` before querying newly published symbols.
Queries use the last completed index during a refresh. Before the first
successful scan, queries return `503` with `index_not_ready`. A failed scan
preserves the prior index and exposes its error through index status.

## Endpoints and execution rules

| Method and path | Purpose |
|---|---|
| `GET /v1/symbols` | Search the global index by fully qualified name and optional filters |
| `POST /v1/extractions` | Queue extraction for a registered file |
| `POST /v1/matches` | Queue a Clang DSL query for a registered file |
| `POST /v1/dependencies` | Queue dependency analysis for a registered file |
| `GET /v1/index` | Inspect global index readiness and background refresh |
| `GET /health` | Ping the HTTP server |
| `GET /v1/jobs`, `GET /v1/jobs/{id}` | List retained jobs or fetch one result |
| `DELETE /v1/jobs/{id}` | Cancel queued work; running native analysis returns `409` |
| `GET /v1/watch` | Inspect watcher state and recent job IDs |
| `GET /openapi.yaml`, `GET /openapi.json` | Read the generated OpenAPI contract |
| `POST /v1/shutdown` | Request orderly server shutdown |

Database resolution, Clang analysis and symbol queries run on background workers.
HTTP requests remain asynchronous. Database-mutating work is serialized; this
does not imply parallel Clang extraction. Native work already running completes
before orderly shutdown. It cannot be forcibly cancelled safely; cancellation
of a running resource job returns `409`. Queued resource jobs can be cancelled.

`GET /v1/jobs` returns metadata with `result: null`; fetch the individual job
for its complete result. Large result serialization runs on workers while HTTP
remains responsive. Job records are in memory and may be evicted or lost at restart. Facts and the
global index persist. Invalid JSON and malformed requests return `400` or `422`;
unknown files and jobs return `404`; ambiguous selectors return `409`; unavailable
index/project state returns `503`; queue saturation returns `429`. Authentication
failures return `401`, and browser-origin requests return `403`. HTTP bodies are
limited to 1 MiB, headers to 16 KiB, with a 30-second request I/O deadline.
Selector resolution failures occur in the accepted job's structured `error`.

## Deprecated command compatibility

`GET /v1/commands`, `POST /v1/commands/{path}` and `POST /v1/jobs` remain for
existing command clients. They are deprecated compatibility operations, separate
from the resource API above. They accept CLI token arrays and return captured
`stdout`/`stderr` and an exit code. A command's JSON output remains a string in
`stdout`. Nested command names use slashes, such as `repo/add-clone`.

Compatibility jobs run as child processes. Their cancellation and configured
`--timeout` terminate the process group. Each output stream is capped at 4 MiB;
`truncated` signals discarded output. Legacy options and database overrides
belong only to these compatibility endpoints. New clients should use the typed
symbol and analysis endpoints.

For synchronous and asynchronous Python examples, see
[Python REST client](../05-python-sdk/11-rest-client.md).
