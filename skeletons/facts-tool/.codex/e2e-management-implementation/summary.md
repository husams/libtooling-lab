# Catalog implementation verification — 2026-08-26

## Result

The requested repository, component, and directory commands are implemented.
The original 23 acceptance failures came from unsupported command groups.
They are now covered by a larger set of **67 real-executable catalog BDD cases**;
all pass on both platforms. The complete E2E gate is green.

| Platform | All BDD cases | Passed | Failed/errors | Skipped | CLI contract |
| --- | ---: | ---: | ---: | ---: | --- |
| macOS 26.1, arm64; Clang/LLVM 22.1.8 | 147 | 146 | 0 | 1 | Passed |
| Linux-dev: Rocky Linux 9.8, x86_64; GCC 15.2.1, Clang/LLVM 21.1.8 | 147 | 147 | 0 | 0 | Passed |

The macOS skip is the existing GNU g++/libstdc++ driver scenario; no matching
GNU driver is available there. It passes on Linux-dev. Rocky is RHEL-compatible,
not a Red Hat Enterprise Linux installation.

Final run durations: macOS BDD 15.638s;
Linux-dev BDD 29.571s.
Both CTest E2E gates passed 2/2 registered tests.

## Evidence

- [macOS CTest log](macos-final/ctest.log), [BDD JUnit](macos-final/bdd.xml),
  [CTest JUnit](macos-final/ctest.xml), [build log](macos-final-build.log).
- [Linux-dev CTest log](linux-dev-final/ctest.log),
  [BDD JUnit](linux-dev-final/bdd.xml),
  [CTest JUnit](linux-dev-final/ctest.xml), [build log](linux-dev-final/build.log).
- [Source SHA-256 manifest](source.sha256): 300 files covering CMake, AGENTS,
  all source, scripts, tests, and fixtures (excluding Python bytecode).
- Manifest SHA-256: `800669ed14a6a50438e4549a367ecc95658a1ec1dda51ef511e5260679e5b6ce`.
- Local Git HEAD: `3e5b5bd6a7cafded0ce1665f0c2ffc33a3422826`, plus the working-tree
  changes. This is not a committed revision of the implementation.
- Linux source/build snapshot: `/tmp/facts-tool-bdd-1NKkld`.
  `sha256sum --quiet -c implementation-source.sha256` passed against all 300 files.
  Local files were also rechecked against the manifest after final E2E completion.
- The saved `/home/husam/libtooling-lab` checkout on Linux-dev remains clean.
  No commits or pushes were made.

Commands used:

```bash
cmake --build build -j4
bash scripts/run-e2e.sh build .codex/e2e-management-implementation/macos-final
```

On Linux-dev, inside the isolated snapshot:

```bash
source /opt/rh/gcc-toolset-15/enable
cmake --build build -j4
bash scripts/run-e2e.sh build implementation-final-evidence
```

The Linux build uses its existing configured SQLite dependency, Python3.9
pytest-bdd environment, and cached CLI11/itlib sources. Both executable E2E gates
run without inherited pytest filters.

## Implementation and test boundaries

The [command guide](../../docs/e2e-project-management.md) documents the surface.
CLI parsing, dispatch, command handling, presentation, and storage operations are
separate modules. New implementation files are at most 105 lines; the existing
CLI parser shrank to 162 lines. The eight feature files and seven step modules
reuse two small support modules (at most 80 lines).

The coverage includes fresh registration, Git root discovery, repository clones,
path/label switches with real re-extraction, versions, JSON command export,
exact scoped removal, read-only previews, missing/duplicate/ambiguous inputs,
and rollback after late SQL failures. Assertions read committed SQLite state
through fresh connections and protect original checkout/facts bytes.

One original acceptance fixture attempted to register `external`, a name already
created by import. Successful registration now uses `vendor`; a separate
scenario explicitly verifies rejection of the built-in `external` duplicate.
No assertion was loosened to make duplicate registration succeed.

Formatting checks for all changed/new CLI and catalog C++ files and
`git diff --check` passed. Semantic cidx qualification remains unavailable
because the prior index refresh failed; this is runtime/build verification.

## Additional native-suite diagnostics — not a passing qualification

The requested executable E2E gate is green; **the broader native suite is not
certified green**.

- With the original Release flags on macOS, 19/20 non-E2E CTest tests passed;
  `fact-store` aborted. See [original native JUnit](macos-native.xml).
- Inspection found native setup calls such as `pipe()` and `waitpid()` inside
  `assert(...)`, which `-DNDEBUG` suppresses. A temporary `-UNDEBUG` experiment
  exposed further failures: macOS `fact-store`, `project-configuration`,
  `reference-extraction`; Linux `fact-store`, `reference-extraction`.
- The macOS project-configuration diagnostic assumes its fixture is discovered
  as `cpp-indexer`; the other diagnostics include allocation/illegal-instruction
  aborts. Their complete root causes were not established in this task.
- [macOS assertion-enabled log](macos-native-assertions.log),
  [Linux assertion-enabled log](linux-dev/native-assertions.log).
- The temporary assertion-flag change was reverted. The final CMake diff only
  adds the new implementation sources. Final E2E reruns above use those original
  production and native-test flags. No native-test source was changed for these
  diagnostics.

The older `../e2e-management-evidence/` reports retain the initial red baseline.
The results above supersede those reports for catalog E2E readiness.

## Pre-merge revalidation — 2026-08-26

Rebuilt and reran the full executable E2E gate on top of local main
`38aa988467868d5a31cc6fb0e90c751a09c0b29f` before committing this feature.

- macOS: 146 passed, 1 existing GNU-driver skip; CLI contract passed.
- Linux-dev: 147 passed, no skips; CLI contract passed.
- [macOS log](pre-merge-macos/ctest.log), [macOS BDD XML](pre-merge-macos/bdd.xml).
- [Linux log](pre-merge-linux/ctest.log), [Linux BDD XML](pre-merge-linux/bdd.xml).
- [Current source manifest](pre-merge-source.sha256): all 307 current source,
  build, test, and fixture files matched on Linux and were rechecked locally.
- The first Linux rerun lacked the newer main commit's module/CUDA fixtures;
  the complete fixture tree was copied and the full gate passed as recorded above.
- Copied build-log trailing whitespace was normalized for Git; diagnostic text
  and all exit/test results are unchanged.
- Database/index artifacts and unrelated working-tree files are excluded.
  This is local main integration; no remote push is requested or performed.
