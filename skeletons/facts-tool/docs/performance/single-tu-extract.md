# Single translation unit extraction: 2026-09-09

The Clang-heavy `src/ast/visitors/BodyVisitor.cpp` improved from **6.919 s to
5.924 s median (14.4% less wall time)** in the final balanced comparison. The
smaller `src/storage/SchemaMigration.cpp` improved from 0.942 s to 0.910 s in
that batch (3.4%), but was unchanged in the initial batch; a general improvement
on small TUs is not established.

## Change and evidence

`extractOverrideRelations` computed a file identity even after `extractLocation`
had rejected a system-header or invalid location. It now returns immediately
when that location is filtered, before filesystem canonicalization and registry
lookup. Valid method target resolution and override extraction are unchanged.
The production change is three added lines and one removed line in the existing
extractor; it introduces no cache, schema change, traversal, or module.

macOS `sample` attributed 592/4195 samples (14.1%) to override extraction before
the change, mostly resolving filtered methods' file paths; afterward it accounted
for 22/5305 samples (0.4%). These separately sampled runs diagnose the bottleneck;
their wall times are excluded from the timing comparison.

## Measurement conditions

- Apple M4, 10 logical CPUs, macOS 26.1, Homebrew LLVM/Clang 22.1.8.
- Both binaries built in Release from base `9e3fa8b15828115ffcd0aa6c6d501b334c4c5d5b`;
  candidate differs only by the early return.
- Original build-emitted compile commands, one selected TU per invocation,
  same imported project database per comparison, explicit output paths.
- One excluded warmup per binary, then six measured runs each, alternating
  baseline/candidate order. Filesystem caches are warm; each facts DB is new.
- Import/setup is outside the timer; wall time includes the entire `extract`
  process and commit. Final timings followed completion of extraction/build/test
  jobs. Desktop services remained active; absolute times varied between batches.
- `FACTS_TOOL_TIMING=1`; user configuration/environment overrides are isolated.

| TU | Baseline wall (s) | Candidate wall (s) | Baseline AST phase (s) | Candidate AST phase (s) |
| --- | ---: | ---: | ---: | ---: |
| BodyVisitor.cpp | 6.919 | 5.924 | 5.897 | 4.899 |
| SchemaMigration.cpp | 0.942 | 0.910 | 0.408 | 0.365 |

All table values are medians. Internal phases overlap with total timing and
must not be added to it. Compare within each batch: the initial five-run batch
showed 4.982 s to 4.247 s (14.8%) for BodyVisitor and 0.676 s to 0.675 s for
SchemaMigration. The final six-run batch balances which binary runs first.

Measured wall seconds, in each binary's run order:

```text
BodyVisitor baseline:  6.846802 6.815488 6.964537 7.032738 6.897757 6.940140
BodyVisitor candidate: 5.916489 5.931004 5.860605 5.984433 6.231560 5.714520
SchemaMigration baseline:  0.931315 0.922365 0.943183 0.940882 1.011065 0.947371
SchemaMigration candidate: 0.891557 0.942272 0.911813 0.908018 0.903593 0.918835
```

Binary SHA-256 identities:

```text
baseline:  07f87d2cdade810841ab902bd0912de51ba06a269f789dd826a03c682c1f9719
candidate: 42359f260dfbdba9edf3849020e83be897b366d07f5dfa8c85b9104f5a7833a6
```

## Reproduce

Preserve a baseline Release binary before rebuilding the candidate. Import the
selected source with its actual build compilation database, using an isolated
configuration directory and an explicit project database. Then run:

```sh
python3 scripts/benchmark-extract.py \
  --baseline /path/to/baseline-facts-tool --candidate build/facts-tool \
  --project /path/to/project.db --source /absolute/path/to/source.cpp \
  --output /path/to/new-benchmark-directory --runs 6
PYTHONPATH=python/src python3 scripts/compare-extract.py \
  --baseline /path/to/new-benchmark-directory/baseline-1.db \
  --candidate /path/to/new-benchmark-directory/candidate-1.db \
  --project /path/to/project.db
```

The benchmark retains fresh databases, logs, exact commands, binary hashes,
source/project hashes, individual stage/wall measurements, and medians in
`results.json`; output must be a new directory. It rejects inputs changed during
measurement. The parity script requires the public `facts_tool` SDK.

## Correctness

All six before/after pairs for both real TUs match across all 13 SDK facts views,
comparing full exposed
semantic rows after replacing generated packed symbol identities with USRs.
Unmaterialized type IDs are retained exactly. This checks exposed SDK facts;
it does not claim byte-identical databases or raw-table parity.

BodyVisitor retains 1674 SDK symbols, 1972 edges, 1006 sites, 658 definitions,
584 parameters, and all enumerator/template/initializer/return-type facts.
SchemaMigration retains 211 symbols, 262 edges, 154 sites, 102 definitions,
67 parameters, and its corresponding side facts. Expression/source-region
views are empty for ordinary extraction in both binaries.

The new native BDD scenario asserts that filtered system-header methods cause
no file-resolution calls during AST extraction while project facts commit.
It passes on the candidate and fails on the preserved baseline on the two
unnecessary header lookups. Existing override/dispatch tests cover valid methods.

## Validation and integration

- Final integrated macOS gate: **40/40 CTest**, including **816 BDD scenarios
  passed, 1 platform skip**, plus 28 batch tests; 230.41 s total.
- Final RHEL gates: **40/40 CTest twice**, including **817 BDD scenarios and
  28 batch tests** each; 291.39 s and 289.71 s. Rocky Linux 9.8 x86_64,
  gcc-toolset-15, LLVM/Clang 21.1.8, fresh isolated build with static SQLite/yaml.
- Independent review approved the production change and regression; the
  baseline mutation check demonstrates that the regression detects the old work.
- The integrated Release binary is installed at `~/.local/bin/facts-tool`;
  installed-command extraction and SDK parity also pass.

The new fixture required adding its two filenames to the existing exact registry
inventory. Earlier simultaneous macOS suites overlapped another ten-worker
extraction job and hit cancellation-test timeouts; the successful final run used
two pytest workers. Those earlier failed logs remain available with the final
evidence rather than being counted as successful qualification.

Local artifacts: `/tmp/facts-extract-perf/`, notably `body-final/results.json`,
`storage-final/results.json`, `parity-final.json`, `ctest-qualified-integrated.log`,
and `rhel/`. RHEL source snapshot and original hashed logs:
`linux-dev:/home/husam/facts-tool-extract-perf-rhel-20260909T062016Z/`.
