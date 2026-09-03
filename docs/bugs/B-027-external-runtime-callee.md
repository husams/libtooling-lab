# B-027 — Preserve valid external runtime callees

## Problem

Call-site extraction can resolve a compiler-visible external declaration and
produce a stable Clang USR while the facts database has no corresponding symbol
row. The observed target is aligned global `operator delete`:

```text
c:@F@operator delete#*v#l#$@N@std@E@align_val_t#
```

The missing database row is currently reported as `InvalidUsr`, aborting the
translation unit and rolling back unrelated valid facts.

## Required behavior

Use one fallible relation-target resolution pipeline:

1. Generate the target declaration USR.
2. Preserve the B-019 policy when Clang cannot generate a non-empty USR: trace
   and filter that unidentifiable declaration without inventing an identity.
3. Look up a successfully generated USR in the facts database.
4. When that valid USR is absent, insert a canonical symbol using the existing
   external-symbol classification and USR uniqueness contract.
5. Resolve its canonical identifier and persist the call site or relation.
6. Propagate metadata and persistence failures after a valid USR so genuine
   failures retain transactional rollback.

A valid-USR database miss must never use the B-019 filtered path. Enabling
`shouldVisitImplicitCode()` is not a fix because the call graph already exposes
the implicit runtime call.

## Reproducer

```cpp
#include <string>

void reproduce() {
  std::string value = "temporary";
}
```

## Implementation

Relation extraction now uses `resolveRelationTarget` for calls, uses, and
overrides. The pipeline filters only declarations for which Clang cannot
generate a USR; a valid missing USR is persisted through the canonical external
symbol path. Before persistence, the target is advanced to its most recent
redeclaration so compiler-created declarations with invalid source locations
can use the visible standard-library declaration and its registered file.

Lookup, file-resolution, and persistence failures after USR generation are
reported as relation-target failures and retain translation-unit rollback.

## Tests

Extend `tests/e2e/features/external_targets.feature` with scenarios proving:

- extraction commits without the call-site `InvalidUsr` diagnostic;
- the exact aligned-delete USR has one externally classified symbol row;
- the originating call-site relation targets that row;
- a second translation unit reuses the symbol and creates no duplicate edge;
- a genuinely un-USR-able declaration retains B-019 skip/trace behavior and
  never enters the external-symbol insertion path;
- the full `facts-tool-e2e` CTest target passes without failures or skips.

## Scope constraints

The resolver belongs in a small `RelationTarget.{h,cpp}` unit shared by call,
reference, and override relation extraction. No modified source file may add
more than 100 lines, and B-027-specific step definitions must stay under 100
lines by reusing the existing common and external-target helpers.
