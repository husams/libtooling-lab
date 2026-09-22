# Server validation — 2026-09-21

The configured instance manages `/home/husam/libtooling-lab/skeletons/facts-tool`
and listens on **http://127.0.0.1:33431**. All **161 registered translation units**
were extracted and indexed. Monitoring is enabled. The server may be performing
its automatic reconciliation after settings restoration; use the status command
for current activity. The final captured state is in
[final-state.json](../.server-runtime/evidence/final-state.json).

**526 of 529 checks passed in the final recorded suite runs.** There are two
reproducible functional defects and one startup-readiness test failure. The
startup scenario passes with an initialization wait. The defect tests remain
ordinary failing tests; they were not skipped or marked expected failures.

| Suite | Checks | Passed | Failed/errors | Evidence |
|---|---:|---:|---:|---|
| Python REST client | 193 | 193 | 0 | [XML](../.server-runtime/evidence/python-rest.xml) |
| Native Python client integration | 5 | 5 | 0 | [XML](../.server-runtime/evidence/python-native.xml) |
| Python callgraph/variable-flow readers | 6 | 6 | 0 | [XML](../.server-runtime/evidence/python-analysis.xml) |
| Shipped live API regression suite | 307 | 306 | 1 | [XML](../.server-runtime/evidence/api-clean.xml) |
| Configured-instance acceptance | 18 | 16 | 2 | [XML](../.server-runtime/evidence/instance-complete.xml) |

The live OpenAPI contract exposes 43 v2 paths and 71 method/path operations.
See [the saved contract](../.server-runtime/evidence/openapi.json) and
[operation inventory](../.server-runtime/evidence/v2-operations.json). These
counts describe the contract, not a claim of exhaustive coverage of every
possible request, interleaving, platform, or resource-exhaustion condition.

## Verified behavior

- Real checkout: extraction, forced refresh, symbol search, AST matching,
  dependencies, calls from `facts::config::detail::mergeTiers`, and variable
  tracking of its `base` parameter.
- Typed sync/async clients: catalogs, symbols, lazy pagination, extraction,
  match results, graph nodes/edges, and job waiting.
- Controlled fixtures: exact/prefix/USR lookup, case sensitivity, occurrences,
  incremental skip counts, custom AST binding names, source capture, include
  dependencies, callers/callees, cross-file calls, graph paths and limits.
- Variable tracking: forward intra/interprocedural flow, argument and return
  transfers, depth boundaries, shadowed-local disambiguation by declaration,
  and rejection of backward tracking.
- CLI-created additional facts database: initial symbol absent from global
  search; API rebuild processes one changed facts source and publishes it.
  A repeated rebuild processes zero sources and preserves the revision.
  Re-extracting renamed content through the CLI and rebuilding removes the old
  symbol and publishes the new one. Compilation import registers the new file.
- Shipped regression suite: catalog updates/deletion, clone switching,
  authentication, HTTP/OpenAPI contracts, cancellation and retries, WAL-only
  facts updates, atomic index failure/recovery, watcher source/header changes,
  atomic saves, newly discovered files, broken links, ignore rules, missing
  headers, malformed compilation databases, compiler overrides, and restart
  fingerprint reuse.
- Lifecycle scripts: repeated start/stop, duplicate instance rejection,
  graceful shutdown, new PID after restart, same saved port, persistent facts
  and symbol queries, and restoration of monitoring.

The detailed instance bundles are in
[acceptance-20260921T204927](../.server-runtime/evidence/acceptance-20260921T204927/). Test fixtures and reports from
unsuccessful earlier attempts were retained rather than overwritten.

## Functional defects

1. **Typed symbol relations fail on a built-in return type.**
   `client.symbols.relations(run_id, direction="both").collect()` raises
   `ProtocolError: Expected <class 'str'>, received NoneType` for `int run()`.
   The HTTP response is successful and includes a `return_type` relation whose
   target is `int` with `symbol_id: null`. The OpenAPI `V2RelatedSymbol` schema
   explicitly permits null, but the SDK's `RelatedSymbol.symbol_id` is typed
   as `str`. This is a confirmed SDK/contract mismatch.
   [Raw response](../.server-runtime/evidence/acceptance-20260921T204927/relations-wire.json).
   The raw HTTP response remains readable; unfiltered typed relation traversal
   remains broken for this case.

2. **Creating a file in an imported component root returns HTTP 409.**
   A component containing two imported sources and their header has registered
   directories `.` and the empty path. Creating another existing source in
   that same directory through `client.files.create(...)` returns
   `resource_conflict: file is outside every registered indexed directory`.
   [Registered directory evidence](../.server-runtime/evidence/acceptance-20260921T204927/file-create-input.json)
   and [error response](../.server-runtime/evidence/acceptance-20260921T204927/file-create-error.json).
   Native `owningDirectory` path containment is the likely source; this is an
   inference from its treatment of normalized root paths. Compilation-database
   import successfully registers the same kind of new source and was used to
   finish the CLI/index and shadow-variable scenarios.

Reproduce these on the running instance:

```bash
.server-runtime/venv/bin/python -m pytest tests/server -v \
  -k 'typed_symbol_relations or file_creation_in_imported_component_root'
```

## Startup and operational findings

The shipped `test_managed_default_storage_supports_subsequent_import_cycles`
checks `/health`, then immediately creates a repository. In the clean run the
POST returned `503 service_not_ready` while initialization was incomplete.
The unchanged remainder of that scenario passed when its server factory first
waited for `GET /api/v2/repositories` to return 200, including subsequent import,
restart reuse, and recovery of deleted managed facts storage.
[Original failure](../.server-runtime/evidence/api-clean.log),
[guarded rerun](../.server-runtime/evidence/startup-recheck.json).
The start script now waits for catalog initialization before reporting success;
this does not imply that repository extraction has finished.

An early suite run placed intentionally Git-less fixtures inside this checkout
and encountered watcher setup failures. The final suite uses external temporary
fixtures, whose location is recorded in
[api-clean-fixtures-location.txt](../.server-runtime/evidence/api-clean-fixtures-location.txt).
One isolated watcher test also failed in that early run and passed in both
standalone reproductions. Its precise early cause was not established.

Initial repository reconciliation completed successfully, then scheduled an
additional cycle after catalog changes. During reconciliation, one repository
status read exceeded the SDK's default 10-second timeout. The traceback is
retained in [bootstrap.log](../.server-runtime/evidence/bootstrap.log).
A subsequent health call completed normally. Operational helpers now use a
60-second HTTP timeout, and status output shows indexed/registered counts plus
watcher activity. This observation is not a throughput or latency guarantee.

Temporary acceptance repositories were unregistered. Their sources, diagnostic
bundles, and facts artifacts remain for investigation. Only `facts-tool` remains
registered in the final instance. Runtime data is ignored by Git.

## Failure investigation and logging

A deliberate missing-header extraction retained a structured error with source
line/column diagnostics, clone/repository, input and effective compiler commands,
configuration discovery, relevant environment, failure stage, expected condition,
and recovery action. Matching `job.failed`, `job.diagnostic`, and `job.context`
records were found by job ID. Repair and retry succeeded with a new ID while the
original failure remained readable.

- [Exported failure before restart](../.server-runtime/evidence/exported-failed-job.json)
- [Log recovery after restart](../.server-runtime/evidence/exported-failed-job-after-restart.json)
- [Lifecycle checks](../.server-runtime/evidence/lifecycle.json)
- [Current failed matcher job, scoped to its creation time](../.server-runtime/evidence/exported-live-failed-job.json)

The exporter filters current-job log records by creation timestamp, so reused
job IDs after restart do not mix earlier attempts into the current bundle.
If the job is unavailable, it labels recovered records as historical matches.
A new deliberate matcher failure (`d1`) remains available for live inspection.

Logs appended across restart and included `server.stopping` and `server.stopped`.
The regression suite verified severity filtering, log destination failures,
secret exclusion, and duplicate-instance log protection. Job records were lost
on restart as documented, so export evidence before restart or retention eviction.
Log rotation and job-history durability are not built-in server features.

## Operation and provenance

See [server operations](server-operations.md) for start/stop, Python examples,
manual reindexing, diagnostic export, and repeatable tests. The managed repository
is at commit `b61f86a44100e93a723a9a07061eae96136e11b3`. The server is built from the
separate source snapshot under `.server-runtime/source`; it has no independent
Git metadata. Its source-tree hash, executable hash, compiler, and SDK version
0.3.0 are recorded in [provenance.json](../.server-runtime/evidence/provenance.json).
The local SDK successfully opened real extracted schema-14 facts and queried
symbols. [Read-only SDK evidence](../.server-runtime/evidence/local-sdk-read.json).
