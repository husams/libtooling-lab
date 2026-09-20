# Architecture and testing

← [User guide index](../README.md) · [Table of contents](../toc.md)

The REST layer lives in `src/apis`, divided by responsibility:

| Directory | Responsibility |
|---|---|
| `config` | CLI server options, YAML persistence and normalized paths |
| `daemon` | Instance locking, background startup and readiness |
| `logging` | Severity filtering, bounded event queue and asynchronous log file writes |
| `http` | Asynchronous listener, HTTP sessions, routing and validation |
| `openapi` | Authoritative OpenAPI 3.1 YAML, split into paths and schemas |
| `generated` | Contract-generated native routes, limits and embedded document |
| `jobs` | Bounded queue, native work scheduling and legacy subprocess lifetime |
| `domain` | Registered-file selection, server configuration and facts-location resolution |
| `operations` | Typed extraction, match and dependency services shared with native command handlers |
| `index` | Persistent global symbol index, background refresh and paged lookup |
| `runtime` | Async resource scheduling, worker execution and index lifecycle |
| `watch` | Inotify events, asynchronous scans, debounce and refresh scheduling |
| `watch/catalog` | Read registered repositories/clones and apply repository/clone exclusions |
| `watch/ignore` | Git ignore precedence, tracked-file rules and YAML path exclusions |
| `watch/plan` | Select eligible source files and build identity-preserving command batches |

Every new C++ source/header and Python test module is kept within 100 lines.
`Server.cpp` connects these components and manages signals and shutdown.
The [class reference](07-class-reference.md) lists native and Python REST types,
including internal implementation types and source files.

Boost.Asio runs asynchronous sockets, timers and inotify; Beast parses and writes
HTTP. Resource handlers validate typed requests and return accepted jobs before
expensive work starts. Worker threads resolve registered identities and invoke
shared native extraction, matching and dependency services. There is no CLI-token
translation in the resource endpoints. A per-invocation clone context resolves
an explicitly selected clone without changing the repository's persisted active
clone. Symbol queries and result JSON serialization run on background executors
too. Retained results are stored separately from small job metadata; listing jobs
does not copy their result data on the HTTP event loop.

For headers without stored compile commands, a background resolver verifies
registered including translation units and groups equivalent compilation
contexts. An invocation-local scope supplies the unique context to native
analysis without changing stored compiler options. Conflicting contexts and
headers with no registered includer produce structured job failures.

The deprecated command compatibility API retains isolated `posix_spawn`
processes, CLI discovery and captured output. It supports existing clients while
new clients use symbol and file resources.

The [OpenAPI contract](08-openapi-contract.md) generates the native route table
and Python clients. Native handlers implement each operation; async Python
methods await HTTPX requests. Both live contract formats are cached at startup.
The generator's `--check` mode verifies that committed bindings match the YAML.

Structured server events enter a bounded queue and are written by a dedicated
logging worker, keeping file writes off the HTTP event loop. Shutdown drains
queued records. See [Logging and verbosity](09-logging.md) for configuration,
event levels and the distinction between server logs and captured command output.

Coordinated queues serialize mutating native, compatibility and watcher work. Native
analysis cannot be interrupted safely after it starts; queued work is cancellable,
while cancellation of running native work returns HTTP `409`. Shutdown waits for
native work to finish. Compatibility process jobs retain process-group cancellation
and deadlines. External CLI processes still follow SQLite concurrency rules.

A background task creates and refreshes `global_symbol_index` in `project.db`
from known facts databases on startup. It stores fully qualified name, kind, USR
and defining file ID, with a composite lookup index. Refresh builds replacement
rows and publishes them transactionally. Queries see a completed generation, and
a failed scan preserves the prior index. Successful extraction/matching and
watcher completion request another refresh after the operation response. Clients
inspect `/v1/index` to distinguish HTTP readiness, job completion and index
publication. Database files remain owned and resolved by the server.

A background watcher scan reads repository and active-clone changes every second.
Path filters apply when registering watches and selecting refresh sources.
Per-clone Git ignore rules reload after relevant control-file changes. Job records
remain in memory; the symbol index and analysis results persist in databases.
There is no persistent job scheduler, WebSocket or live terminal-output stream.

## Run the integration tests

The REST server adds build dependencies on Boost headers version 1.74 or later
and nlohmann/json version 3.9 or later, alongside the existing CLI11 and yaml-cpp
dependencies. Compatible installed packages or headers are preferred; otherwise
the build fetches checksum-verified, pinned header-only sources. The RHEL helper
caches these under `.deps/api-headers`, covering older distribution headers
and headers missing when using `SKIP_DEPS=1`. See the
[installation guide](../01-introduction/03-installation.md#cached-headers-and-offline-builds)
for source overrides and offline preparation.

```sh
python3 -m pip install -r tests/e2e/requirements.txt
python3 scripts/generate_openapi.py --check
python3 -m pytest tests/apis \
  --api-facts-tool /absolute/path/to/build/facts-tool \
  --api-compiler /absolute/path/to/clang++
```

The tests start real servers in temporary directories with isolated configuration.
They check typed symbol lookup, source selectors and structured job results,
then check the compatibility command catalog against CLI help and execute C++ import,
extraction, matching and call-graph workflows, and exercise concurrent clients,
protocol errors, authentication, daemon startup, instance exclusion and restart.
OpenAPI tests validate the source and live JSON/YAML documents, response schemas,
generated routes and limits, encoded command paths, and HTTP responsiveness.
Resource tests cover cross-repository symbols, optional kind/USR filters, startup
indexing, post-extraction/match refresh, clone selection, ambiguity and traversal
errors, async responsiveness and preservation of the previous index on failure.
Logging tests check destination and verbosity precedence, structured event
filtering, daemon readiness and log failures, and redaction of request values.
Linux tests verify source/header edits, same-commit edits with AST caching enabled,
atomic saves, new nested directories and failed reimport reporting. Repository
monitoring scenarios cover database-driven roots, clone switches, exclusions,
nested Git ignore rules and source filtering during automatic refreshes.
The native queue tests cover output limits, timeouts,
cancellation, process cleanup and queue capacity deterministically.

## Run the E2E BDD suites

The native Gherkin scenarios live in `tests/e2e/features/rest_*.feature` and are
included automatically in the existing `facts-tool-e2e` CTest gate. From
`skeletons/facts-tool`, run the complete native BDD and CLI-contract gates:

```sh
bash scripts/run-e2e.sh /absolute/path/to/build
```

These scenarios use real HTTP requests, typed native services and compatibility
workers. They cover global symbols and index readiness, resource jobs, command
discovery, authentication, actual C++ analysis, daemon readiness and instance
locking, saved ports, and Linux inotify refreshes. Watcher scenarios verify that
source/header edits become visible even when the Git commit has not changed.
Contract scenarios exercise both document formats and generated async clients.
Logging scenarios exercise foreground and daemon files, saved settings, severity
filtering and real HTTP/job events.
Inotify scenarios require Linux.

The SDK has separate Gherkin scenarios for both `Client` and `AsyncClient`.
From the same directory, run them with explicit native executable paths:

```sh
cd python
export FACTS_TOOL_NATIVE=/absolute/path/to/build/facts-tool
export FACTS_CLANGXX=/absolute/path/to/clang++
uv sync --locked --extra rest
uv run pytest tests/bdd/rest
uv run python scripts/run_installed_bdd.py
```

SDK scenarios exercise real server jobs, persisted C++ analysis results,
authentication and command failures, polling, cancellation, and async client
behaviour. A client-side timeout or cancelled Python task must leave the remote
job available until it finishes or the client explicitly cancels it.

The installed BDD runner builds a wheel, installs its `rest` extra into an
isolated environment, and runs **all** Python BDD scenarios against that installed
package. It requires both native executable paths and removes inherited pytest
selection options. This complements the SDK's HTTP contract tests, which use
controlled transports to exercise malformed responses and network failures.
