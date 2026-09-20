# Architecture and testing

The REST layer lives in `src/apis`, divided by responsibility:

| Directory | Responsibility |
|---|---|
| `config` | CLI server options, YAML persistence and normalized paths |
| `daemon` | Instance locking, background startup, logs and readiness |
| `http` | Asynchronous listener, HTTP sessions, routing and validation |
| `jobs` | Bounded job queue, subprocess lifetime, output and cancellation |
| `watch` | Recursive inotify registration, events and refresh scheduling |

Every new C++ source/header and Python test module is kept within 100 lines.
`Server.cpp` connects these components and manages signals and shutdown.

Boost.Asio runs asynchronous socket, pipe, timer and inotify operations; Beast
parses and writes HTTP. Long-running commands run in isolated `posix_spawn`
processes using the same executable and CLI parser as normal terminal commands.
This preserves CLI behavior and keeps expensive Clang work off the HTTP event
loop. The command catalog is generated from CLI registrations, including aliases,
so new CLI commands are automatically discoverable through the generic API.

One queue serializes API and watcher command processes to avoid concurrent writes
from this server. External CLI processes or a second server with a different
configuration still follow the existing SQLite concurrency rules. Cancellation
and timeout apply to the worker process group, including compiler subprocesses.

The API is a command service: successful commands persist their existing database
results and return captured terminal output. It does not introduce a second
database schema, a persistent job scheduler, WebSockets or live output streaming.

## Run the integration tests

The REST server adds build dependencies on Boost headers version 1.74 or later
and nlohmann/json version 3.9 or later, alongside the existing CLI11 and yaml-cpp
dependencies. Install `libboost-dev nlohmann-json3-dev` on Ubuntu, or run
`brew install boost nlohmann-json` on macOS. The RHEL build script installs
`boost-devel` and `json-devel`.

```sh
python3 -m pytest tests/apis \
  --api-facts-tool /absolute/path/to/build/facts-tool \
  --api-compiler /absolute/path/to/clang++
```

The tests start real servers in temporary directories with isolated configuration.
They check the live command catalog against CLI help, execute actual C++ import,
extraction, matching and call-graph workflows, and exercise concurrent clients,
protocol errors, authentication, daemon startup, instance exclusion and restart.
Linux tests verify source/header edits, same-commit edits with AST caching enabled,
atomic saves, new nested directories and failed reimport reporting.
The native queue tests cover output limits, timeouts,
cancellation, process cleanup and queue capacity deterministically.
