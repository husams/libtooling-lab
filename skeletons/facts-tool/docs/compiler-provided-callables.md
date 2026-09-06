# Compiler-provided callables (B-034)

Explicit project-owned calls can resolve to Clang's implicit allocation and
deallocation declarations without a physical declaration location. A valid
generated USR is their identity. Resolution first reuses that identity, then
prefers a usable redeclaration, and only routes an implicit `FunctionDecl`
without usable declaration provenance to the compiler-provided storage domain.
There is no operator-name whitelist. Invalid USRs retain B-019 filtering;
missing registered physical files and persistence failures remain errors.

FileId zero reserves `BuiltinType::Kind + 1` for predefined primitives. Dynamic
compiler symbols start at `BuiltinType::LastKind + 2`, or above an occupied
FileId-zero ID or allocator high-water mark, whichever is greater. Allocation
and unique-USR persistence share the existing write transaction. Reopening,
missing/stale allocator rows, both primitive insertion orders, and 32-bit
exhaustion are covered without a new migration or fabricated file. Primitive
numbering follows the linked Clang type model; this change does not redefine
the existing primitive representation between toolchains.

Compiler callables are lightweight Symbol rows with semantic callable kind,
actual USR/name, implicit/external and callable flags. Zero coordinates and no
definition row mean no physical declaration provenance. Real Calls sites retain
the caller's file, line and column. The native graph recognizes lightweight
callable kinds as well as Function rows, and stops at external boundaries.
Ordinary file-backed lightweight symbols retain their previous flag behavior.

## Fresh versus populated output

The pre-fix Dispatch reproduction was output-state dependent because
`extractCallSite` checked a stored caller and resolved its target before
filtering the call site's location. Existing header caller identities allowed
further header targets to be materialized during the callgraph pass. In the
populated run, allocator `allocate_at_least` enrolled `allocate`, whose call to
implicit `operator new` failed resolution even though its system-header site
would subsequently have been discarded. Fresh output lacked those enrolled
callers and avoided the failing lookup.

Site eligibility now precedes target resolution. Excluded system-header sites
cannot cause target writes or target-resolution failures. This preserves the
existing project-site boundary; it does not add CXXNewExpr/CXXDeleteExpr call
extraction or index standard-library bodies. The two-TU system-header fixture
reproduces the old rollback and verifies the corrected ordering with fresh and
populated output. Project-owned explicit calls still exercise compiler-symbol
persistence independently of that ordering fix.

Baseline `bdc818971a4e0ddefdd435baa4a4635cc4a0740a`, freshly built with macOS
arm64 LLVM 22.1.8, reproduces Dispatch exit 0 on fresh output (281 symbols,
27 files) and exit 1 with a consistent populated snapshot. The candidate exits
0 for both (269 symbols, respectively 26 and 28 files touched), retaining 79
fresh Calls sites and 191 total sites in the populated database. Those totals
include prior committed facts and are not whole-program completeness claims.
The populated main graph expands through `facts::cli::run` and dispatch;
external boundaries and the requested depth still limit interpretation.

## Regressions and validation

- `external_targets.feature`: minimal headerless explicit allocation, sibling
  definitions, canonical identity, real site and native graph output.
- `implicit_callables.feature`: scalar/array normal/aligned new; normal,
  sized, aligned and sized-aligned delete; eight `<new>` nothrow overloads;
  observed builtin USR behavior; two TUs, reopen/rerun identity; new-USR trigger
  failure with all-table rollback; primitive allocation; excluded header sites.
- `implicit-target-probe` records the actual generated USR, semantic kind,
  implicit status, redeclaration locations and call coordinates for each cell.
  The headerless aligned fixture declares only the `std::align_val_t` enum;
  no allocation functions or nothrow declarations are invented. Supported
  matrix invocations use C++23 and `-fsized-deallocation` on macOS LLVM 22.1.8
  and RHEL LLVM 21.1.8. No capability skips replace passing coverage.
- `implicit-storage` checks both primitive orders, occupied dynamic IDs,
  allocator loss, reopen, higher watermarks, rollback and overflow;
  `implicit-declaration` checks usable redeclarations and real missing-file
  failures. Existing migration and B-019 scenarios remain unchanged.

Run all native gates with `bash scripts/run-e2e.sh build build/b034/native-e2e`
and `ctest --test-dir build --output-on-failure`; run the complete SDK BDD with
`cd python && uv run pytest tests/bdd`. Use an isolated `XDG_CONFIG_HOME` for
native CLI absence checks and a real GNU driver via `FACTS_GXX` on macOS.
Exact reviewed-commit results, matrix JSON, baseline/candidate logs, database
snapshot hashes and per-file size counts are recorded in the B-034 handoff.

SQLite errors now keep their extended numeric codes in an SQLite category.
The target diagnostic includes name, USR and the SQLite cause, including
constraint failure from a real new-target write; it no longer treats SQLite
codes as operating-system errno values.
