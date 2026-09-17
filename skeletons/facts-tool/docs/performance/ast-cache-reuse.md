# Multi-translation-unit AST cache benchmark

Run the native executable against a newly generated, committed C++ project:

```sh
python3 scripts/benchmark-ast-cache.py \
  --binary /absolute/path/to/facts-tool \
  --compiler /absolute/path/to/g++ \
  --output /absolute/path/to/new-results-directory \
  --translation-units 12 --declarations 64 --runs 7 \
  --build-description "Release, -O3, linked Clang 21.1.8"
```

The output directory must not exist. The script uses only Python's standard
library, Git, the specified C++ driver and the real `facts-tool` executable.
Set `--build-description` to the actual mode, optimization flags and linked
Clang version; the example is not an assumption about your binary. Use the
same runtime/library environment required by that executable. Keep
builds, E2E suites and other expensive work idle while measuring. Increase
`--translation-units` for scaling runs; keep all settings identical when
comparing binaries. `--observe` collects failing baseline evidence without a
nonzero final exit; `results.json` still records `passed: false` and every
violation. Ordinary runs stop immediately when a measured invocation violates
a check and exit nonzero; the partial final report and measurement log are retained.

## Fixture and scope

The default project has 12 translation units in separate nested directories,
all named `unit.cpp`, with relative filenames and relative source arguments in
`compile_commands.json`; the compilation directory is a separate `build`
folder from the invocation directory. They share a header which includes a transitive
header containing 64 template declarations and standard `<array>`, `<utility>`
and `<type_traits>` headers. A cross-TU function chain exercises variable flow
and call-graph recovery. The fixture has real Git commits and explicit,
isolated configuration and database paths.

The regression-validation build used during this change is Debug with `-O0`,
built using GCC 14.2 and linked Clang 21.1.8; its fixture uses host GNU C++ 13.3.
Its absolute timings are validation-build measurements, not estimates for an
optimized production binary. Zero repeated front-end work remains a directly
checked property independent of optimization level.

This is a reproducible synthetic workload, not a production-codebase benchmark
or operating-system qualification. The report records the host and binary
identities. A run on Linux does not qualify RHEL specifically. Filesystem
caches are warm after setup; timings include process startup, configuration,
Git checks, database work, AST integrity hashing/deserialization and the
requested command's traversal/analysis.

## Correctness gates, independent of timing

- Cold import must emit exactly one preprocessing sentinel per TU and save
  exactly one AST per TU. The sentinel is a real `#pragma message` diagnostic;
  counting only diagnostic lines avoids double-counting echoed source text.
- Every first consumer after import must hit the prepared cache. Extract,
  match, variable flow and dependency analysis also exercise absolute,
  relative and dot-relative selectors, plus a nested invocation directory.
  Directory paths and symlink aliases are not accepted by all source registry
  commands and are not treated as supported selectors in this benchmark.
- Every warmed cached invocation must emit zero preprocessing sentinels, run
  zero external GNU compiler probes, report no `frontend:` parse, dependency
  scan or include reconstruction activity, report no cache miss/unavailable/store
  events, and preserve all AST file hashes, modification timestamps and the
  typed dependency/cache and compiler-probe metadata tables. Expected AST and
  dependency hit counts must match the selected TU count exactly.
- GNU compiler discovery is measured independently from TU preprocessing:
  a wrapper around the selected `g++` logs each actual external invocation.
  With a Clang driver there is no GNU probe assertion coverage; the report
  identifies that limitation with `gnu_driver_instrumented: false`.
- A source/header poison control inserts `#error` into every TU and the shared
  transitive header without committing. All cached commands must still succeed
  from their committed snapshot. Cache-disabled forced extraction must fail
  on those current source errors. The files are restored afterward.
- A source commit, shared-header commit and empty commit separately require
  import to rebuild affected snapshots once. The immediate subsequent
  extract, match, variable-flow and dependency invocations must already hit.
  In this single-repository fixture every TU records the same HEAD, so any
  repository commit invalidates every TU by the documented policy.
- Extraction/matching must retain the entry symbol, dependency facts must be
  nonempty, every generated worker must retain its definition, variable flow
  must complete with the full function chain, and call-graph recovery must
  complete without failed attempts and retain every cross-TU chain edge. Recovery starts from a restored incomplete facts
  database on every invocation so a previous run cannot turn the test into a
  database-only graph query. Every Python SQLite connection is explicitly
  closed. Restore removes the owned destination and its WAL/SHM sidecars,
  uses SQLite backup from a read-only seed, verifies zero pre-existing runs,
  and requires the invocation to produce exactly one complete run with ID 1.

These checks establish that source preprocessing and external compiler probes
are not repeated. They do not claim zero AST traversal: each command still
performs its requested work. Cache metadata checks and artifact hashing are
also expected. The separate `frontend: include-reconstruction` event detects an unnecessary
traversal of an already loaded preprocessing record, which the source sentinel
alone cannot observe. The report retains both observations.

## Timing matrix and retained evidence

The script measures warm import, forced extract, unchanged extract, match,
variable flow, call-graph recovery and dependency analysis, each with caching
enabled and disabled. It alternates enabled/disabled order on successive
iterations and excludes one warmup per case. Reports contain all samples,
median and nearest-rank p95; with seven samples p95 is the maximum sample,
not a stable estimate of a population tail. Cache-disabled non-skipped
commands must trigger the preprocessing sentinel, making them positive
controls for the measurement mechanism.

`measurements.jsonl` is flushed after each invocation so interrupted runs retain
raw measurements; `results.json` additionally includes later semantic checks and
is the authoritative completed report. `results.json` includes exact command lines, exit codes, cache event counts,
per-TU preprocessing counts, external driver probe counts, cache mutation
checks, extraction phase timings, binary/compiler hashes, initial cache
inventory and failures. The fixture, databases, per-command logs and GNU
probe log are retained next to the report. Snapshot reads, artifact hashes,
restoring the recovery seed and validation queries happen outside the timed
subprocess interval.

Compare medians only when both runs use the same scale, compiler/runtime,
build optimization and idle host. A faster number never overrides a failed
zero-work or semantic check.

## Validation: 2026-09-17

Both complete scale runs passed: **338 recorded command invocations**, including
196 measured runs and **216 strict warm-cache checks**. Every warm check reported
zero source preprocessing, AST parsing, dependency scans, AST include reconstruction,
external GNU probes, cache misses and cache rewrites. Exact AST/dependency hit counts
matched the selected TU count; unchanged extraction loaded zero ASTs.

The [machine-readable evidence](ast-cache-validation-2026-09-17.json) retains
all labeled samples, phase timings when available, work/hit/mutation/status counters,
identities, scale footprints and summaries without bulky AST metadata or scratch paths.

The frozen candidate was built with GCC 14.2 in **Debug with `-O0`**, linked
against Clang 21.1.8, and exercised with GNU C++ 13.3.0 on Linux x86-64. No
project builds or E2E suites ran concurrently; shared-host scheduling was not
controlled. These are synthetic validation-build measurements, not optimized
production estimates or RHEL qualification. The candidate binary SHA-256 is
`9eb727d59c8ea487bbf93e40d2ab9f7c9536fb1bf55db6f28474ea7d2d6ed661`.

Each scale used 64 shared template declarations, two project headers and
44 transitive external headers. Compile commands used a separate build directory
and relative filenames; consumers covered absolute/relative/dot-relative paths,
nested invocation directories, duplicate/overlapping selectors and all sources.

| TUs | Unique input files | AST artifacts | Serialized AST bytes | Cold import (s) |
| ---: | ---: | ---: | ---: | ---: |
| 16 | 62 | 16 | 33,686,464 | 3.865 |
| 32 | 78 | 32 | 67,372,904 | 9.517 |

Cold import parsed each TU exactly once and ran one GNU probe per project.
Source, shared-header and empty-commit changes each caused one new parse per
TU during import; the immediate consumers were warm. Across both scales this
means 48 initial AST preparations and 144 commit-refresh preparations, with
no separate dependency preprocessing passes during those imports. All 36
recovery invocations started from a verified zero-run seed/destination and
produced exactly complete run 1. Full expected chain coverage was checked
after the first recovery consumer and after every timing iteration.

The following values are whole-process seconds from seven measured samples
per command/mode, after one excluded warmup. Cache-disabled runs are positive
controls against the same binary. With seven samples, nearest-rank p95 is the
maximum observed sample; these values do not establish population tail latency.

### 16 translation units

| Command | Cached median | Cached p95 | Disabled median | Disabled p95 |
| --- | ---: | ---: | ---: | ---: |
| Repeated import | 0.397 | 0.594 | 0.338 | 0.514 |
| Forced extract | 4.919 | 8.342 | 7.104 | 7.753 |
| Unchanged extract | 0.115 | 0.155 | 0.371 | 0.739 |
| Match | 1.038 | 1.580 | 2.448 | 3.731 |
| Variable flow | 1.870 | 2.313 | 3.027 | 3.552 |
| Call-graph recovery | 13.364 | 17.867 | 14.232 | 17.709 |
| Dependency analysis | 0.172 | 0.467 | 0.366 | 0.659 |

### 32 translation units

| Command | Cached median | Cached p95 | Disabled median | Disabled p95 |
| --- | ---: | ---: | ---: | ---: |
| Repeated import | 0.651 | 0.948 | 0.686 | 1.100 |
| Forced extract | 11.431 | 14.303 | 12.929 | 15.525 |
| Unchanged extract | 0.159 | 0.283 | 0.577 | 1.300 |
| Match | 1.734 | 3.706 | 4.195 | 7.155 |
| Variable flow | 3.728 | 4.855 | 5.258 | 7.584 |
| Call-graph recovery | 40.588 | 52.281 | 45.428 | 53.490 |
| Dependency analysis | 0.392 | 0.759 | 0.663 | 1.168 |

Cache reuse does not eliminate command analysis or metadata/artifact checks.
Repeated import was slightly slower with caching in the 16-TU median, and
32-TU cached recovery still took 40.588 seconds median while emitting no
front-end work. The evidence therefore supports elimination of repeated
parsing/discovery, not a guarantee that every command or sample becomes faster.

The preserved PR #88 baseline reproduced two real preprocessing passes for
a relative forced extraction, one for relative dependency analysis, eight
for duplicate/overlapping forced extraction, and an external GNU probe on
every warm process. These observations use source pragma diagnostics and a
real driver wrapper independently of the new telemetry. The corrected
selector and repeated-consumer checks observe zero such work.

An earlier provisional 16-TU run was rejected when the exact-hit assertion
exposed stale SQLite WAL state in the benchmark restoration. Every Python
SQLite connection is now explicitly closed, restoration uses a fresh owned
destination and SQLite backup, and each recovery invocation verifies fresh
run 1. The corrected three-repetition smoke passed 113 invocations before
both full scale runs were repeated. All figures above and in the evidence
file exclude the rejected trial.
