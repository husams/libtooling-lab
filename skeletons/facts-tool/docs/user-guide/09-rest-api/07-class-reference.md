# REST implementation classes

← [User guide index](../README.md) · [Table of contents](../toc.md)

This inventory lists native classes and plain value types, including internal
implementation details, and the exported Python REST classes. Most resolution,
indexing and analysis behavior is implemented as functions.

## Native server

Names are relative to `facts::apis`, including nested namespaces such as
`domain`, `index`, `runtime` and `watch`. Source paths are relative to `src/apis`.

| Class or struct | Responsibility | Source |
|---|---|---|
| `Arguments` | Collect server CLI arguments before merging saved settings. | [config/Options.h](../../../src/apis/config/Options.h) |
| `Settings` | Hold resolved address, port, authentication, job and watch settings. | [config/Settings.h](../../../src/apis/config/Settings.h) |
| `logging::Options` | Hold the independently configured log destination and severity. | [logging/Options.h](../../../src/apis/logging/Options.h) |
| `logging::Logger` | Filter structured events, queue writes and drain the worker on shutdown. | [logging/Logger.h](../../../src/apis/logging/Logger.h) |
| `logging::State` | Own the bounded record queue, worker thread and synchronization state. | [logging/State.h](../../../src/apis/logging/State.h) |
| `logging::Descriptor` | Own and close the logging file descriptor. | [logging/Descriptor.h](../../../src/apis/logging/Descriptor.h) |
| `Lifecycle` | Own instance locking, daemon startup, readiness and cleanup. | [daemon/Lifecycle.h](../../../src/apis/daemon/Lifecycle.h) |
| `Listener` | Bind the TCP endpoint and accept connections asynchronously. | [http/Listener.h](../../../src/apis/http/Listener.h) |
| `Session` | Read, parse and answer one HTTP request asynchronously. | [http/Session.h](../../../src/apis/http/Session.h) |
| `Router` | Authenticate requests and dispatch REST operations. | [http/Router.h](../../../src/apis/http/Router.h) |
| `OpenApiDocuments` | Hold the live JSON and YAML contracts prepared at startup. | [http/OpenApi.h](../../../src/apis/http/OpenApi.h) |
| `MatchedRoute` | Pair a generated operation with its decoded path parameter. | [http/Routing.h](../../../src/apis/http/Routing.h) |
| `HttpError` | Carry an HTTP status and validation or access error. | [http/Routing.h](../../../src/apis/http/Routing.h) |
| `generated::Route` | Describe a method, path template, parameter and operation from the YAML contract. | [generated/Routes.h](../../../src/apis/generated/Routes.h) |
| `domain::Error` | Carry a status classification, machine-readable code, failure message and optional structured details. | [domain/Selection.h](../../../src/apis/domain/Selection.h) |
| `domain::FileSelector` | Identify a source by path and optional repo, clone and component. | [domain/Selection.h](../../../src/apis/domain/Selection.h) |
| `domain::Context` | Hold resolved server-owned project configuration. | [domain/Selection.h](../../../src/apis/domain/Selection.h) |
| `domain::ResolvedFile` | Carry registered file identity, selected clone and internal source/facts paths. | [domain/Selection.h](../../../src/apis/domain/Selection.h) |
| `domain::detail::Candidate` | Hold one catalog identity for selector validation and disambiguation. | [domain/Candidates.h](../../../src/apis/domain/Candidates.h) |
| `operations::ExtractRequest` | Carry typed extraction options. | [operations/Operations.h](../../../src/apis/operations/Operations.h) |
| `operations::MatchRequest` | Carry Clang DSL query, traversal and match capture options. | [operations/Operations.h](../../../src/apis/operations/Operations.h) |
| `operations::compilation::Candidate` | Pair a registered translation-unit file ID with its compile command for header context selection. | [operations/compilation/Candidates.h](../../../src/apis/operations/compilation/Candidates.h) |
| `operations::compilation::Group` | Group equivalent header compilation settings with the source translation units that can supply them. | [operations/compilation/Candidates.h](../../../src/apis/operations/compilation/Candidates.h) |
| `index::Query` | Hold exact-name search filters and pagination inputs. | [index/Index.h](../../../src/apis/index/Index.h) |
| `index::Symbol` | Represent qualified name, kind, USR and defining-file identity. | [index/Index.h](../../../src/apis/index/Index.h) |
| `index::Page` | Hold one completed-generation symbol page and continuation cursor. | [index/Index.h](../../../src/apis/index/Index.h) |
| `index::RefreshResult` | Report published generation, symbol count and scanned facts stores. | [index/Index.h](../../../src/apis/index/Index.h) |
| `index::Position` | Decode a generation-bound pagination position. | [index/Internal.h](../../../src/apis/index/Internal.h) |
| `runtime::Request` | Hold one validated typed operation and file selector. | [runtime/Request.h](../../../src/apis/runtime/Request.h) |
| `runtime::Service` | Expose async resource submission, symbol lookup and index status. | [runtime/Service.h](../../../src/apis/runtime/Service.h) |
| `runtime::Task` | Pair a retained domain job with scheduled native work. | [runtime/State.h](../../../src/apis/runtime/State.h) |
| `runtime::State` | Own resource jobs, worker executors and background index lifecycle. | [runtime/State.h](../../../src/apis/runtime/State.h) |
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
`logging::Level` is an enumeration, not an additional class or struct.
The existing `facts::cli::ImportOptions` and `facts::cli::ExtractOptions`
each have `noAstCache` to support watcher refreshes of uncommitted edits.
`ImportOptions` also has `existingClone` to preserve clone identity during
automatic reimport. The Git repository/index handle aliases are not extra types.

## Shared native service types

| Type | Responsibility | Source |
|---|---|---|
| `facts::ScopedCloneContext` | Apply one invocation's clone mapping and restore the prior mapping without changing the catalog. | [storage/CloneContext.h](../../../src/storage/CloneContext.h) |
| `facts::CompilePathRemapping` | Hold registered active and explicitly selected clone roots for compilation-path remapping. | [tooling/StoredCompilationReader.h](../../../src/tooling/StoredCompilationReader.h) |
| `facts::CompilationContext` | Hold the server project identity and selected compile commands for one analysis invocation. | [tooling/CompilationContext.h](../../../src/tooling/CompilationContext.h) |
| `facts::ScopedCompilationContext` | Apply a selected header compilation context and restore the previous invocation context without changing stored commands. | [tooling/CompilationContext.h](../../../src/tooling/CompilationContext.h) |
| `facts::AnalysisDiagnostic` | Carry bounded compiler diagnostics as severity, message and source location. | [model/AnalysisDiagnostic.h](../../../src/model/AnalysisDiagnostic.h) |
| `facts::DiagnosticScope` | Collect operation-local Clang diagnostics without terminal output. | [tooling/DiagnosticScope.h](../../../src/tooling/DiagnosticScope.h) |
| `facts::commands::match::Preparation` | Resolve selected source/include identities and facts provenance before matcher execution. | [commands/match/MatchPreparation.h](../../../src/commands/match/MatchPreparation.h) |
| `facts::commands::match::MatchOutput` | Collect structured matcher records independently of terminal presentation. | [commands/match/MatchOutput.h](../../../src/commands/match/MatchOutput.h) |

## Python REST SDK

New clients use the following `/api/v2` resource types. Source paths are relative
to `python/src/facts_tool/rest`.

| Class or group | Responsibility | Source |
|---|---|---|
| `Client`, `AsyncClient` | Typed resource collections, discovery, settings and v1 compatibility access | [v2/client.py](../../../python/src/facts_tool/rest/v2/client.py), [v2/async_client.py](../../../python/src/facts_tool/rest/v2/async_client.py) |
| `Repository`, `File`, `Component`, `CompilationCommand`, `NewClone` | Catalog values and nested configuration | [v2/catalog_models.py](../../../python/src/facts_tool/rest/v2/catalog_models.py) |
| `FileSelection`, `FileReference`, `FileIdentity` | Typed source selection | [v2/selections.py](../../../python/src/facts_tool/rest/v2/selections.py) |
| `GlobalSymbol`, symbol occurrence and relation models | Stable symbol identities and source evidence | [v2/symbol_models.py](../../../python/src/facts_tool/rest/v2/symbol_models.py) |
| `AnalysisJob`, `AsyncAnalysisJob` | Operation-specific results, waiting and cooperative cancellation | [v2/jobs.py](../../../python/src/facts_tool/rest/v2/jobs.py), [v2/async_jobs.py](../../../python/src/facts_tool/rest/v2/async_jobs.py) |
| `ExtractionResult` and dependency/import/scan results | Typed analysis summaries and records | [v2/analysis_models.py](../../../python/src/facts_tool/rest/v2/analysis_models.py) |
| Graph and variable-flow models | Typed nodes, edges, boundaries and coverage | [v2/graph_models.py](../../../python/src/facts_tool/rest/v2/graph_models.py), [v2/flow_models.py](../../../python/src/facts_tool/rest/v2/flow_models.py) |

V2 models and HTTP adapters are maintained alongside the generated contract and
validated with native response and SDK tests. Existing v1 exports remain:


| Class | Responsibility | Source |
|---|---|---|
| `LegacyClient` | Provide synchronous v1 resources and job polling. | [client.py](../../../python/src/facts_tool/rest/client.py) |
| `LegacyAsyncClient` | Provide asynchronous v1 resources and job polling. | [async_client.py](../../../python/src/facts_tool/rest/async_client.py) |
| `FileSelector` | Identify a registered source using path and optional repo, clone and component. | [domain_models.py](../../../python/src/facts_tool/rest/domain_models.py) |
| `Symbol` | Represent one global-index result with defining file identity. | [domain_models.py](../../../python/src/facts_tool/rest/domain_models.py) |
| `SymbolPage` | Hold a tuple of symbols and optional next cursor. | [domain_models.py](../../../python/src/facts_tool/rest/domain_models.py) |
| `IndexStatus` | Report index readiness, pending work, counts and failure. | [domain_models.py](../../../python/src/facts_tool/rest/domain_models.py) |
| `DomainJob` | Represent native operation state and a structured result or error. | [domain_models.py](../../../python/src/facts_tool/rest/domain_models.py) |
| `OperationError` | Hold a domain failure code, message and optional structured details. | [domain_models.py](../../../python/src/facts_tool/rest/domain_models.py) |
| `Job` | Represent an immutable job snapshot and check completion or failure. | [models.py](../../../python/src/facts_tool/rest/models.py) |
| `ApiError` | Report HTTP rejection with status code and server message. | [errors.py](../../../python/src/facts_tool/rest/errors.py) |
| `TransportError` | Report failure before receiving an HTTP response. | [errors.py](../../../python/src/facts_tool/rest/errors.py) |
| `ProtocolError` | Report a successful HTTP response that violates the API contract. | [errors.py](../../../python/src/facts_tool/rest/errors.py) |
| `JobFailedError` | Retain a completed failed or cancelled job and its output. | [errors.py](../../../python/src/facts_tool/rest/errors.py) |
| `JobTimeoutError` | Report an expired wait while the remote job continues. | [errors.py](../../../python/src/facts_tool/rest/errors.py) |

V1 native resource jobs decode as `DomainJob`; compatibility jobs decode as
`Job`. V2 jobs use their operation-specific result model. The v1 client classes
are generated from the OpenAPI contract and small
lifecycle/polling templates. Their resource bindings are split into two internal
base classes to keep generated modules small:

| Internal class | Responsibility | Source |
|---|---|---|
| `ResourceOperations` | Generated synchronous v1 methods. | [generated/resources.py](../../../python/src/facts_tool/rest/generated/resources.py) |
| `AsyncResourceOperations` | Generated async v1 methods. | [generated/async_resources.py](../../../python/src/facts_tool/rest/generated/async_resources.py) |

## Components implemented as functions

File selection, facts-path and header-compilation-context resolution, native resource operations, index refresh
and search, server orchestration, configuration parsing and persistence, command discovery,
OpenAPI document expansion, catalog reading, exclusion selection, watcher path discovery
and AST-cache invalidation use free functions. There is no `Server` class.
Python argument validation, configuration,
response decoding, HTTP transport and polling also use functions, without extra
class hierarchies. See [Architecture and testing](04-architecture-and-testing.md)
for execution and test coverage, [OpenAPI generation](08-openapi-contract.md),
and [Python REST client](../05-python-sdk/11-rest-client.md)
for SDK usage.
