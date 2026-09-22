# REST batch failure collection — verified 2026-09-22

The updated server is running at `http://127.0.0.1:33431`, managing `facts-tool`.
Final health is `ok`, the global index is ready, all **161/161 translation units**
are indexed, and watching is enabled with zero watcher failures.

Validation used the server snapshot in `.server-runtime/source` and its editable
Python SDK. The running binary and native regression executable have the same
SHA-256: `37bfb05001191037064491ddd05c6967c90b0c783c86fc1b7a74b1140eab0cdf`.
The validated implementation is now tracked directly in `src/apis` and
`python/src/facts_tool/rest/v2`, with the OpenAPI contract and regression tests.
The snapshot baseline matched commit `5817b08` on `main` for every patched file.

## Behavior added

- `continue_on_error` on REST extraction, matching and dependency jobs, plus the
  synchronous and asynchronous Python clients. It defaults to false.
- Paginated `failed_files` records with file ID, path, error code/message and full
  retained diagnostic context. Python exposes `resource.failed_files(job_id)`.
- Accurate processed, unchanged/skipped, failed and not-attempted counts.
- Mixed batches report `coverage=partial`; batches with every file failing have
  terminal state `failed`. Cancellation still stops processing.
- Partial results remain readable after failure/cancellation when analysis began.
- Failed and partially failed batches can be retried without changing the original
  job's records. Retrying uses current compilation settings.
- Per-file failure/context/diagnostic logs identify both job and file; completion
  logs include counts and coverage. The inspection script exports all collections.

Selection errors, cancellation and global index publication failures remain fatal.
Corrupt facts databases are not silently skipped by global reindex, and legacy
watcher command batches retain their existing stop-on-error behavior. Job history
remains in-memory and is lost on restart; exported evidence and logs persist.

## Manual verification on this repository

The controlled run temporarily added a nonexistent `-include` file to the stored
commands for `ConfigurationMerge.cpp` and `ConfigurationPaths.cpp`. It selected
those files followed by `ConfigurationSearch.cpp`, ensuring the healthy file came
**after** both failures. All original commands were restored; source hashes were
unchanged. Successful re-extraction restored their facts and index state.

| Case | Observed result |
|---|---|
| Default extraction, `d1` | Failed; 1 failed, 2 not attempted; failure collection readable |
| Continued extraction, `d2` | 2 failures collected, third file processed, 286 symbols written |
| Continued matching, `d3` | 2 failures collected, third file processed, 2 named-function matches |
| Continued dependencies, `d4` | 2 failures collected, third file processed, 816 edges |
| Every file fails, `d5` | Failed; both errors retained, zero not attempted |
| Async extraction, `d6` | Same partial outcome; two failure pages decoded by the async SDK |
| Cancellation, `d7` | Cancelled; 161 selected, 161 not attempted; counts preserved |
| Restored extraction, `d8` | All 3 processed, zero failures, complete coverage |
| Retry after repair, `d9` | All 3 processed; `retry_of=d2`; original two failure records unchanged |
| CLI updates existing facts, REST index `d11` | One source processed; next reindex processed zero |
| CLI creates an additional facts DB, REST index `d14` | New DB indexed; one source processed and old source association removed |
| Restore original facts association, `d16` | Original DB reattached through CLI and reindexed successfully |
| Investigation tool | `d2`: 2 failed-file records and 47 correlated log records; `d5`: 2 and 56 |

The new-database test used the real `ConfigurationSearch.cpp`, the same project
catalog and defaults, and a separate output DB. It verified symbol queries after
reindex, a no-change reindex, and restoration of the original DB association.

Evidence is under `.server-runtime/evidence/`:

- `continuation-live-20260922T021504/`: full job summaries, failures, successful
  records, diagnostics, source hashes, command restoration and correlated logs.
- `continuation-cli-added-20260922T021832/`: additional DB extraction, REST index
  summaries and original association restoration.
- `continue-inspect-partial.json`, `continue-inspect-failed.json`: exported
  investigation bundles from the running instance.
- `continue-final-state.json`, `continue-provenance.json`: health/catalog/watcher
  snapshots and binary hashes.

Repeat with `.server-runtime/venv/bin/python scripts/test-server-continuation.py`.
The script restores commands and watcher settings in `finally` blocks.

## Automated checks

- **199 Python REST tests passed**, including six new sync/async continuation and
  failure-pagination checks (`continue-final-sdk.xml`).
- The **315 native API tests** produced **314 passes and one readiness-test race**
  (`continue-final-regression.xml`). The failing test submitted repository creation
  before asynchronous catalog initialization, receiving the expected temporary
  HTTP 503. It now waits for index/catalog initialization and passes on rerun
  (`continue-readiness-fix.xml`). The other 314 tests remain unchanged by that fix.
- Eight native continuation tests cover multiple failures, a missing source,
  successful later-file publication, pagination/schema validation, full log-context
  reconstruction, repair/retry, default stop behavior, invalid flags and all-failed
  status. They passed in the native suite.
- OpenAPI generation and `generate_openapi.py --check` passed. The implementation was applied cleanly to the current `main` sources.

An earlier regression run overlapped executable relinking and had temporary launch
permission errors. That run is superseded by the stable-executable run above.
The live log reader was also corrected to tolerate daemon startup text mixed with
JSON log records.

A broad `functionDecl().bind("f")` matcher exposed an existing persistence error
for an implicit lambda in `ConfigurationSearch.cpp` (invalid source location).
The failure was correctly collected; its evidence is in
`continuation-live-20260922T021012/match.json`. The final continuation check used
`functionDecl(hasName("facts::config::resolve")).bind("f")` to isolate the feature.
That matcher limitation and the earlier unrelated defects in
[the original validation report](server-validation.md) are not fixed by this patch.

## Publication check against main

The implementation was applied to a branch based on `5817b08` (`origin/main`).
All 1,183 native source files and 175 Python source files, including regenerated
OpenAPI bindings, matched the validated runtime snapshot byte for byte. From that
branch, the 199 Python REST tests and 10 focused native tests (continuation,
diagnostics and the corrected startup-readiness test) all passed. OpenAPI's
`--check`, shell syntax checks and `git diff --check` also passed.
