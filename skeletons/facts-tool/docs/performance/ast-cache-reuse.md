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
