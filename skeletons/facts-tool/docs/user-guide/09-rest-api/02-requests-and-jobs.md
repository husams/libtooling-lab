# REST resources and asynchronous jobs

← [User guide index](../README.md)

Use `/api/v2` for new clients. The server owns project databases, facts stores,
AST caches and the global symbol index. Requests identify repositories, files,
symbols and analyses; clients never pass database paths or CLI command arrays.
The existing `/v1` API remains available for compatibility.

```sh
API=http://127.0.0.1:42817
```

Use the listener address written by `serve`. Add
`-H "Authorization: Bearer $FACTS_TOOL_API_TOKEN"` when authentication is enabled.
Paths in request bodies refer to the **server's** filesystem.

## HTTP methods

| Method | Meaning | Parameters |
|---|---|---|
| `GET` | Read a resource or collection | Resource IDs in paths; filters and pagination in the query |
| `POST` | Register a resource or start a job | A typed JSON body |
| `PATCH` | Change selected fields | A typed JSON body; omitted fields remain unchanged |
| `PUT` | Replace a complete configuration | All required configuration fields in JSON |
| `DELETE` | Unregister a resource or cancel a job | Resource ID in the path |

`GET` and `DELETE` do not use JSON bodies. Version and addressed resource IDs are
in the URL. Request bodies do not repeat an API version or an operation name.
IDs returned in JSON let clients address resources. Treat them as opaque strings.
Unknown request fields are rejected instead of silently ignored.

## Repositories and automatic import

Register a repository and its first clone:

```sh
curl -sS -X POST "$API/api/v2/repositories" \
  -H 'Content-Type: application/json' \
  -d '{"name":"example","clones":[{"label":"main","path":"/workspace/example"}]}'
```

The first clone becomes active. Registration returns `201 Created`, the
repository object and its `Location`. When watching is enabled, the server scans
the active clone, discovers eligible `compile_commands.json` files, imports
commands and extracts registered sources. The same initial reconciliation runs
when the server starts. Registration does not wait for indexing; inspect
`GET /api/v2/watcher` and `GET /api/v2/index` for progress and failures. The
repository also exposes `source_count` and `indexed_source_count` for registered
translation units with compilation commands. A file with no stored compiler
command, such as a header using an includer context, returns
`compilation_command: null`.

Compilation database generation remains the build system's responsibility.
Without a discovered database, automatic processing uses stored compilation
commands. Sources without compilation settings cannot be analysed. Monitoring,
ignore rules, explicit compilation database paths and startup behavior are
covered in [Repository monitoring](03-watching-directories.md).

Clones are fields of their repository, not separate REST resources:

```sh
curl -sS -X PATCH "$API/api/v2/repositories/$REPOSITORY_ID" \
  -H 'Content-Type: application/json' \
  -d '{"active_clone_id":"clone-2"}'
```

Use the actual returned clone ID. Supplying `clones` replaces the registered
clone list atomically. Include an existing clone's `id` to retain its identity;
omit `id` for a new clone. Removing the active clone requires selecting another
in the same update. Changing registrations never deletes source directories.

Compilation settings belong to the file resource:

```sh
curl -sS -X PATCH "$API/api/v2/files/$FILE_ID" \
  -H 'Content-Type: application/json' \
  -d '{"compilation_command":{"driver":"clang++","working_directory":"/workspace/example/build","arguments":["-std=c++23","-I../include"]}}'
```

The complete `compilation_command` replaces the old settings and invalidates
affected cached analysis. Automatic server reimports preserve this explicit
file override. The `arguments` array contains compiler options; it
is not a facts-tool command invocation.

## Symbol lookup

Only a name or USR is required. Names use **case-sensitive literal prefix
matching by default**, so this finds `example::Widget` and longer names beginning
with it:

```sh
curl -sS -G "$API/api/v2/symbols" \
  --data-urlencode 'qualified_name=example::Widget' \
  --data-urlencode 'kind=class'
```

Add `--data-urlencode 'match=exact'` for name equality. `%` and `_` in the input
are ordinary characters, not wildcard operators. A USR always selects by exact
identity. Optional `repository`, `component` and `kind` filters narrow results.
Overloads and symbols in different repositories remain separate records.

The result is a typed page with `items`, `next_cursor` and `index_revision`.
Symbol items have `symbol_id`, `qualified_name`, `kind`, `usr`, `repository`,
`component`, and a nullable `definition` location. Use:

- `GET /api/v2/symbols/{id}` for one symbol.
- `GET /api/v2/symbols/{id}/occurrences` for declarations and definitions.
- `GET /api/v2/symbols/{id}/relations` for relationships, with optional `kind` and
  `direction=outgoing|incoming|both`.

`limit` defaults to 50 and is at most 500 for symbols. Follow `next_cursor` using
the original filters. Symbol cursors identify an index revision; after revision
expiry, restart from the first page. A missing name returns an empty page; a
missing resource ID returns `404`. Before the initial index is available, queries
return `503 index_not_ready`, not a misleading empty result.

## Select source files

Extraction, matching and dependencies take a discriminated `selection`:

```json
{
  "selection": {
    "type": "files",
    "files": [
      {"path": "src/Widget.cpp", "repository": "example"}
    ]
  }
}
```

A file reference can instead be `{"file_id":"the-returned-file-id"}`. Path
references accept optional `repository`, `clone` and `component` selectors. A
relative path is resolved in the selected clone or component. An omitted clone
uses the repository's active clone. Ambiguous selections return a structured
error; the server does not choose an arbitrary source.

Other selection variants are:

| Selection | Fields |
|---|---|
| `directory` | `path`, optional `repository` |
| `component` | `component`, optional `repository` |
| `repository` | `repository` |
| `all` | No other fields; selecting all registered sources is explicit |

Registered headers use an including translation unit's compilation context.
Conflicting includer settings or missing context produce a structured analysis
error; clients do not supply an ad hoc database or guessed compiler flags.

## Create, read and cancel jobs

Each analysis has its own job collection and operation-specific OpenAPI request
and result schemas:

| Analysis | Collection |
|---|---|
| Extraction | `/api/v2/extract/job` |
| AST matching | `/api/v2/match/job` |
| Manual compilation import | `/api/v2/import/job` |
| Dependencies | `/api/v2/dependencies/job` |
| Call graph | `/api/v2/callgraphs/job` |
| Local-variable flow | `/api/v2/variable-flow/job` |
| Directory scan | `/api/v2/scan/job` |
| Global index rebuild | `/api/v2/index/job` |

For each collection, `POST` starts work and `GET` lists jobs. For
`{collection}/{id}`, `GET` reads the job and `DELETE` requests cancellation.
`GET {collection}/{id}/results` reads record pages; filtering and pagination use
query parameters. Job reads contain counts and scalar summaries, so polling a
large graph never downloads its nodes and edges. Retrieve those lazily with
`?collection=nodes`, `?collection=edges`, or another documented collection. A job ID belongs to its analysis family: asking another
family for it returns `404`.

Start extraction:

```sh
curl -i -X POST "$API/api/v2/extract/job" \
  -H 'Content-Type: application/json' \
  -d '{"selection":{"type":"files","files":[{"path":"src/Widget.cpp","repository":"example"}]},"force":false}'
```

The response is `202 Accepted` with a `Location` header pointing to the job.
Poll that URL until the state is terminal. Queued and running jobs have no final
result yet. Errors contain a stable code and message. Compiler diagnostics are retrieved
through the typed `diagnostics` result collection where available. Extracted facts are published to the global index before a
successful extraction job reports completion.

Cancellation uses `DELETE`, not a `/cancel` command. Jobs remain readable for
retained history. Cancellation must not corrupt an in-progress native database
transaction; inspect the returned state or conflict error rather than assuming
that HTTP cancellation killed the underlying Clang operation. Job retention is
bounded and job records are not durable across restart; facts and indexes are.

## Match the AST

```sh
curl -sS -X POST "$API/api/v2/match/job" \
  -H 'Content-Type: application/json' \
  -d '{"selection":{"type":"repository","repository":"example"},"expression":"cxxMethodDecl(hasName(\"run\")).bind(\"method\")","traversal":"IgnoreUnlessSpelledInSource","capture_source":true}'
```

The full Clang matcher DSL and arbitrary binding names remain available.
Optional `bindings` maps semantic roles to those names; it does not impose a
fixed name on every matcher. Results contain typed matched-node records with
source locations and ranges. Prefix search defaults for the symbols collection
do not alter Clang matcher semantics.

## Call graphs and local-variable tracking

Start a call graph from one exact function identity:

```json
{
  "root": {"qualified_name": "example::Service::run"},
  "direction": "callees",
  "max_depth": 10
}
```

Send this to `POST /api/v2/callgraphs/job`. Root and optional target selectors
accept a qualified name, USR, or returned `symbol_id`; ambiguous function names
require a more precise identity. `direction` can be `callers` or `callees`.
Explicit node, edge, time and depth limits control traversal. Typed results
report coverage and truncation rather than suggesting that bounded output is
complete.

Track a local variable across calls:

```json
{
  "function": {"qualified_name": "example::Service::run"},
  "variable": {
    "name": "request",
    "declaration": {"path": "src/Service.cpp", "line": 42, "column": 9}
  },
  "direction": "forward",
  "interprocedural": true,
  "max_call_depth": 10
}
```

Send this to `POST /api/v2/variable-flow/job`. Declaration location distinguishes
same-name variables in different scopes. The current implementation supports
forward tracking only; backward requests are rejected. Set `interprocedural`
to `false` to stay within the selected function and report call-depth boundaries.
Diagnostics identify unresolved calls, uncertain aliasing and coverage limits;
the analysis is not a claim of complete proof.

## Resource endpoint reference

| Resource | Collection methods | Individual methods |
|---|---|---|
| `/repositories` | `GET`, `POST` | `GET`, `PATCH`, `DELETE` |
| `/components` | `GET`, `POST` | `GET`, `PATCH`, `DELETE` |
| `/files` | `GET`, `POST` | `GET`, `PATCH`, `DELETE` |
| `/directories` | `GET` | `GET`, `DELETE` |
| `/symbols` | `GET` | `GET` |

Prefix paths with `/api/v2`; individual paths append `/{id}`. Catalog collection
pages use `items` and `next_cursor`, `limit=50` by default, maximum 500. Deletion
with dependent registrations requires `cascade=true`; source files are never
deleted. Watcher settings support `GET`, full replacement with `PUT`, or partial
update with `PATCH` at `/api/v2/watcher/settings`.

Other endpoints are `GET /api/v2/health`, `/readiness`, `/index`, `/watcher`,
`/settings`, `POST /api/v2/shutdown`, and `GET /openapi.yaml` or `/openapi.json`.
Readiness is separate from HTTP liveness. Invalid bodies return `400` or `422`,
missing resources `404`, conflicts `409`, queue saturation `429`, and unavailable
initial state `503`. Authenticated servers return `401` for missing/invalid
credentials. HTTP bodies are limited to 1 MiB.

The deprecated `/v1/commands` and generic `/v1/jobs` compatibility operations
still accept CLI token arrays and captured process output. No `/api/v2` endpoint
uses that command-wrapper contract. See [Python REST client](../05-python-sdk/11-rest-client.md).
