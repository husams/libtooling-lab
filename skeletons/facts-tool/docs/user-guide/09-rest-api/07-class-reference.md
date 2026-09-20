# REST implementation classes

← [User guide index](../README.md) · [Table of contents](../toc.md)

The REST server and Python wrapper include 30 C++ classes/structs and eight
Python classes. This inventory includes internal implementation types; only the
Python classes below form the public Python REST interface.

## Native server

Names are relative to `facts::apis`; names prefixed with `watch::` belong to
`facts::apis::watch`. Source paths are relative to `src/apis`.

| Class or struct | Responsibility | Source |
|---|---|---|
| `Arguments` | Collect server CLI arguments before merging saved settings. | [config/Options.h](../../../src/apis/config/Options.h) |
| `Settings` | Hold resolved address, port, authentication, job and watch settings. | [config/Settings.h](../../../src/apis/config/Settings.h) |
| `Lifecycle` | Own instance locking, daemon startup, readiness and cleanup. | [daemon/Lifecycle.h](../../../src/apis/daemon/Lifecycle.h) |
| `Listener` | Bind the TCP endpoint and accept connections asynchronously. | [http/Listener.h](../../../src/apis/http/Listener.h) |
| `Session` | Read, parse and answer one HTTP request asynchronously. | [http/Session.h](../../../src/apis/http/Session.h) |
| `Router` | Authenticate requests and dispatch REST operations. | [http/Router.h](../../../src/apis/http/Router.h) |
| `Job` | Hold a native job record and its completion callback. | [jobs/Job.h](../../../src/apis/jobs/Job.h) |
| `Queue` | Expose job submission, listing, lookup, cancellation and shutdown. | [jobs/Queue.h](../../../src/apis/jobs/Queue.h) |
| `Queue::Impl` | Keep queue implementation state behind its public interface. | [jobs/Queue.cpp](../../../src/apis/jobs/Queue.cpp) |
| `QueueState` | Serialize workers, retain job records and enforce queue limits. | [jobs/QueueState.h](../../../src/apis/jobs/QueueState.h) |
| `Process` | Run a CLI child and manage asynchronous output, timeout and cancellation. | [jobs/Process.h](../../../src/apis/jobs/Process.h) |
| `Process::Stream` | Track one captured stdout/stderr pipe, buffer and truncation flag. | [jobs/Process.h](../../../src/apis/jobs/Process.h) |
| `ProcessResult` | Carry exit status, captured output and termination flags. | [jobs/Process.h](../../../src/apis/jobs/Process.h) |
| `Child` | Return the spawned process identifier and output descriptors. | [jobs/Spawn.h](../../../src/apis/jobs/Spawn.h) |
| `Pipes` (file-local) | Close pipe descriptors automatically on spawn failures or completion. | [jobs/Spawn.cpp](../../../src/apis/jobs/Spawn.cpp) |
| `SpawnOptions` (file-local) | Initialize and release POSIX spawn actions and attributes. | [jobs/Spawn.cpp](../../../src/apis/jobs/Spawn.cpp) |
| `Watcher` | Expose watcher startup, shutdown and status. | [watch/Watcher.h](../../../src/apis/watch/Watcher.h) |
| `Watcher::Impl` | Manage recursive inotify watches, catalog polling, debounce and filtered refresh jobs. | [watch/State.h](../../../src/apis/watch/State.h) |
| `watch::Scan` | Hold a catalog snapshot, monitored roots/directories, control files, compilation databases and notices. | [watch/Scan.h](../../../src/apis/watch/Scan.h) |
| `watch::Clone` | Describe one registered clone, its repository, active status and exclusion reason. | [watch/catalog/Catalog.h](../../../src/apis/watch/catalog/Catalog.h) |
| `watch::Catalog` | Hold a resolved project database path and its clone snapshot. | [watch/catalog/Catalog.h](../../../src/apis/watch/catalog/Catalog.h) |
| `watch::Ignore` | Apply per-clone Git ignore rules, tracked-file semantics and YAML exclusions. | [watch/ignore/Ignore.h](../../../src/apis/watch/ignore/Ignore.h) |
| `watch::Ignore::Impl` | Own libgit2 rule/index handles, layered rule caches and normalized excluded directories. | [watch/ignore/State.h](../../../src/apis/watch/ignore/State.h) |
| `watch::ignore::Rules` | Hold paired libgit2 matchers for one ignore file's positive and negated rules. | [watch/ignore/Rules.h](../../../src/apis/watch/ignore/Rules.h) |
| `watch::ignore::Layered` | Apply nested ignore-file precedence and cache path decisions. | [watch/ignore/Layered.h](../../../src/apis/watch/ignore/Layered.h) |
| `watch::Event` | Carry a changed path and directory/control-file flags to the scanner. | [watch/Update.h](../../../src/apis/watch/Update.h) |
| `watch::Update` | Return an updated snapshot, accepted event count and optional refresh plan. | [watch/Update.h](../../../src/apis/watch/Update.h) |
| `watch::Plan` | Hold filtered import and extraction command batches. | [watch/plan/Plan.h](../../../src/apis/watch/plan/Plan.h) |
| `watch::plan::Options` | Hold validated automatic-job options and explicit compilation database paths. | [watch/plan/Details.h](../../../src/apis/watch/plan/Details.h) |
| `watch::plan::Compilation` | Pair a compilation database directory with its source entries. | [watch/plan/Details.h](../../../src/apis/watch/plan/Details.h) |

`Pipes` and `SpawnOptions` belong to an anonymous namespace inside `facts::apis`.
The forward declarations of implementation structs are not additional types.
The existing `facts::cli::ImportOptions` and `facts::cli::ExtractOptions`
each have `noAstCache` to support watcher refreshes of uncommitted edits.
`ImportOptions` also has `existingClone` to preserve clone identity during
automatic reimport. The Git repository/index handle aliases are not extra types.

## Python REST SDK

All eight classes are exported from `facts_tool.rest`. Source paths are relative
to `python/src/facts_tool/rest`.

| Class | Responsibility | Source |
|---|---|---|
| `Client` | Provide synchronous REST operations, command execution and job polling. | [client.py](../../../python/src/facts_tool/rest/client.py) |
| `AsyncClient` | Provide asynchronous REST operations and cancellation-aware polling. | [async_client.py](../../../python/src/facts_tool/rest/async_client.py) |
| `Job` | Represent an immutable job snapshot and check completion or failure. | [models.py](../../../python/src/facts_tool/rest/models.py) |
| `ApiError` | Report HTTP rejection with status code and server message. | [errors.py](../../../python/src/facts_tool/rest/errors.py) |
| `TransportError` | Report failure before receiving an HTTP response. | [errors.py](../../../python/src/facts_tool/rest/errors.py) |
| `ProtocolError` | Report a successful HTTP response that violates the API contract. | [errors.py](../../../python/src/facts_tool/rest/errors.py) |
| `JobFailedError` | Retain a completed failed or cancelled job and its output. | [errors.py](../../../python/src/facts_tool/rest/errors.py) |
| `JobTimeoutError` | Report an expired wait while the remote job continues. | [errors.py](../../../python/src/facts_tool/rest/errors.py) |

The native and Python `Job` types are separate representations of the same REST
record. `JobState` is a Python type alias, not an additional class.

## Components implemented as functions

Server orchestration, configuration parsing and persistence, command discovery,
OpenAPI generation, catalog reading, exclusion selection, watcher path discovery
and AST-cache invalidation use free functions. There is no `Server` class.
Python argument validation, configuration,
response decoding, HTTP transport and polling also use functions, without extra
class hierarchies. See [Architecture and testing](04-architecture-and-testing.md)
for execution and test coverage, and [Python REST client](../05-python-sdk/11-rest-client.md)
for SDK usage.
