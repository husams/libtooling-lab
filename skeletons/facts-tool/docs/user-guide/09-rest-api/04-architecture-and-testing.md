# Architecture and testing

← [User guide index](../README.md) · [Table of contents](../toc.md)

The REST layer lives in `src/apis`, divided by responsibility:

| Directory | Responsibility |
|---|---|
| `config` | CLI server options, YAML persistence and normalized paths |
| `daemon` | Instance locking, background startup, logs and readiness |
| `http` | Asynchronous listener, HTTP sessions, routing and validation |
| `openapi` | Authoritative OpenAPI 3.1 YAML, split into paths and schemas |
| `generated` | Contract-generated native routes, limits and embedded document |
| `jobs` | Bounded job queue, subprocess lifetime, output and cancellation |
| `watch` | Inotify events, asynchronous scans, debounce and refresh scheduling |
| `watch/catalog` | Read registered repositories/clones and apply repository/clone exclusions |
| `watch/ignore` | Git ignore precedence, tracked-file rules and YAML path exclusions |
| `watch/plan` | Select eligible source files and build identity-preserving command batches |

Every new C++ source/header and Python test module is kept within 100 lines.
`Server.cpp` connects these components and manages signals and shutdown.
The [class reference](07-class-reference.md) lists native types and eight
Python REST classes, including internal implementation types and source files.

Boost.Asio runs asynchronous socket, pipe, timer and inotify operations; Beast
parses and writes HTTP. Long-running commands run in isolated `posix_spawn`
processes using the same executable and CLI parser as normal terminal commands.
This preserves CLI behavior and keeps expensive Clang work off the HTTP event
loop. The command catalog is generated from CLI registrations, including aliases,
so new CLI commands are automatically discoverable through the generic API.

The [OpenAPI contract](08-openapi-contract.md) generates the native route table
and Python clients. Native handlers implement each operation; async Python
methods await HTTPX requests. Both live contract formats are cached at startup.
The generator's `--check` mode verifies that committed bindings match the YAML.

One queue serializes API and watcher command processes to avoid concurrent writes
from this server. A background scan reads repository and active-clone changes
from the catalog every second. Path filters are applied both when registering
watches and when selecting automatic import/extraction sources. Per-clone Git
ignore rules and the index are reloaded after relevant control-file changes.
External CLI processes or a second server with a different
configuration still follow the existing SQLite concurrency rules. Cancellation
and timeout apply to the worker process group, including compiler subprocesses.

The API is a command service: successful commands persist their existing database
results and return captured terminal output. It does not introduce a second
database schema, a persistent job scheduler, WebSockets or live output streaming.

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
They check the live command catalog against CLI help, execute actual C++ import,
extraction, matching and call-graph workflows, and exercise concurrent clients,
protocol errors, authentication, daemon startup, instance exclusion and restart.
OpenAPI tests validate the source and live JSON/YAML documents, response schemas,
generated routes and limits, encoded command paths, and HTTP responsiveness.
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

These scenarios use real HTTP requests and native CLI workers. They cover command
discovery, authentication, actual C++ analysis, daemon readiness and instance
locking, saved ports, and Linux inotify refreshes. Watcher scenarios verify that
source/header edits become visible even when the Git commit has not changed.
Contract scenarios exercise both document formats and generated async clients.
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
