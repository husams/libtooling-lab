# Limitations and known issues

This chapter separates three kinds of gaps: **deliberate exclusions** (design
choices, not bugs), **known bugs** still affecting the current build, and
**call-graph completeness caveats** (things a "complete" traversal status
does not promise). See [in-flight F-013](05-in-flight-f-013.md) for work
that addresses some of this but is not yet on `main`.

## Deliberate exclusions

- **System headers are excluded from location-bearing symbol extraction.**
  Long-standing design; interacts with several of the bugs below because a
  member's *owner record* can legitimately live in a filtered header.
- **`analyse call-graph` prints no text, JSON, or Mermaid output.** As of a
  B-042 rewrite, it persists an append-only run and prints exactly one
  completion line. Some older wiki research pages show Mermaid diagrams or
  `complete=true truncated=0` text output - those describe a CLI surface
  that **no longer exists on main**; don't use them as a reference for
  current behavior. See [call graphs](../04-call-graphs/01-overview.md).
- **The YAML configuration schema is exactly four keys** (`conf_root`,
  `conf_template`, `facts_template`, `extra_args`) - no booleans, no
  numbers, and nothing else (verbosity, selectors, matcher expressions,
  depth) is YAML-configurable. This is intentional, not an oversight.
- **`import` always creates its own component** from the compilation
  database directory's basename, and never reuses an existing component of
  the same name even when told to via `--component NAME=PATH`. See "Catalog
  component pitfalls" below - this is the single most consequential
  behavior for anyone designing a catalog workflow.
- **Ordinary `extract` does not build or maintain call-graph analysis
  structures.** Extraction-time `Calls`/`Overrides`/`DispatchCalls`
  relations are recorded once during normal extraction; the graph-analysis
  passes (`analyse call-graph`) are a separate, explicitly requested step.
  This was a deliberate correction (Backlog B-041) to keep `extract` from
  gaining call-graph-specific enrichment.

## Known bugs affecting the current build

| ID | Status | Symptom | Detail |
|---|---|---|---|
| B-019 | **Apparently still open** - no recorded fix; confirm against live Backlog before treating this as permanently fixed or permanently broken | A class-template specialization named without being required complete (`TSK_Undeclared`) elsewhere in a translation unit makes the **entire extraction roll back to zero symbols**, exit 1 | `facts-tool: indexing incomplete: cannot persist relation=template_instance ... usr='<unavailable>': Invalid argument`. A single such declaration (e.g. `Holder<Widget> *p;`, a reference-only parameter, an alias, or a pointer field) elsewhere in the file zeroes out an otherwise-successful extraction. Separately, an anonymous field is misclassified as `result=failure reason='invalid USR'` instead of being filtered, per the same bug's fixture B. |
| B-020 | **Fixed** | Dependent `alignof(T)` (or similar) inside an uninstantiated template used to crash the extractor (SIGSEGV, exit 139) | `evaluatedValue()` now rejects value-/type-/instantiation-dependent expressions before evaluating, persisting the written text with `evaluated_kind='none'`. |
| B-021 | **Fixed** | Two independent defects: (1) an out-of-line member/field/enumerator whose owner record lives in a filtered system header used to abort extraction with "target symbol is not persisted"; (2) a reference through an anonymous union/struct field used to be a fatal invalid-USR error | (1) now gets a persisted `is_external=1` stub keyed by USR, reconciled later if a real definition appears. (2) is now correctly skipped, keyed on `!referenced.getDeclName()` rather than the printed name. |
| - | Historical, dated 2026-08-19, **status not re-verified against current main** | `facts-tool: cannot persist extracted facts: No such file or directory` actually meant an inheritance-relation target that is a filtered system-header base class could not be found - a misleading ENOENT-shaped message | May be superseded by the B-021 owner-stub fix above (which covers `method_of`/`field_of`/`enumeration` owners), but this specific finding is about a `Specializes`/`inherits`-adjacent path, not exactly the same code path. Verify with a live repro before citing it as a current defect. |
| - | Doc bug, not a code bug | `python/docs/databases.md` and `python/docs/troubleshooting.md` say only facts `user_version` 10 and 11 are supported | The actual code (`schema.py`) supports 10, 11, and 12. See [storage schema](02-storage-schema.md). |
| - | Doc bug, not a code bug | `python/docs/quickstart.md` and `python/docs/cidx-migration.md` each describe a different "ambiguous symbol ref" behavior, and neither matches the implementation | See [troubleshooting](03-troubleshooting.md#ambiguous-symbol-refs-are-not-an-error-contradicts-some-docs). |
| - | Dead code, not fatal | `queryplan.helpers.is_template()` never matches real data | See [troubleshooting](03-troubleshooting.md#is_template-is-dead-code-on-real-data). |

## Catalog component pitfalls

These are current, reproducible behaviors of `import`'s implicit component
creation, not edge cases:

1. **Pre-registering a component and then running `import -p DIR` without
   `--component` creates two overlapping components for the same files.**
   `repo add demo PROJ` -> `component add --path PROJ --name demo --repo demo
   --kind repo` -> `import -c project.db -p PROJ` (no `--component`) ->
   `extract` on the imported sources fails with `facts-tool: ambiguous
   stored compile commands for requested source '...'`.
2. **`import --component NAME=PATH` never reuses an existing component of
   that name - it creates a duplicate**, and `component show`/`list` then
   fail on the resulting orphan (`component has no active clone: <name>` or
   `ambiguous component`).
3. **The cleanest working pattern is: `repo add` (for clone tracking)
   followed by plain `import -p DIR`, with no manual `component add` and no
   `--component` flag on `import`.** This still leaves exactly one
   component whose `component list`/`component show` fail with "no active
   clone" - but nothing else in the workflow (`dir list`, `file list`,
   `extract`, `symbol`, `match`, `analyse call-graph`) depends on those two
   commands succeeding. A user who runs `component list` after any plain
   `import -p DIR` should expect this error; it is not a sign of
   misconfiguration.
4. Whether the "no active clone" failure is a tracked defect or an accepted
   current limitation of `import`'s component creation was not resolved in
   the research behind this guide - check Backlog/issue history before
   presenting a workaround as permanent.
5. A **built-in component** (`kind=external`, root `/`, id `1`) already
   exists in a project database as soon as the first `repo`/`component`/
   `import` command creates that database. It is named `facts-tool` at
   creation and is renamed in place to `external` by the first `import`
   that registers out-of-project headers. Its runtime origin and intended
   purpose beyond "duplicate-name rejection in tests" is not documented
   for end users.

## Call-graph completeness caveats

A call-graph run's `status=complete` means the requested traversal exhausted
everything reachable from the roots under the given scope and budget - it
does **not** mean:

- **every function has been extracted** (see "function entry" in
  [entries, runs, and status](../04-call-graphs/03-entries-runs-and-status.md));
- **stored facts are fresh** relative to the file on disk right now
  (freshness is a separate, largely *unknown* axis unless explicitly
  validated by a recovery attempt);
- **every callee was resolved to a concrete definition.**

Specific caveats:

- **Virtual dispatch is conservative, not exact.** `DispatchCalls` (kind 18)
  may include targets that are not actually reachable at runtime for a given
  static type, rather than under-approximating. It never silently drops a
  plausible override.
- **External boundaries are a complete stop, not a truncation.** A call
  target whose declaration is known but whose definition is not in the
  project (or not yet extracted) records a `callgraph_external_reference`
  row and is reported as `complete`, with no frontier row - distinct from a
  budget stop.
- **Indirect/unresolved calls have no destination row at all.** They are
  recorded separately in `callgraph_unresolved_site`, with no destination
  field - the traversal never invents an external target for an indirect
  call.
- **Runtime callees the static analysis cannot see (function pointers,
  `std::function`, virtual calls through an unproven receiver) never
  appear as edges** unless Clang's own call-graph construction could
  statically resolve them; the analysis is source/AST-level, never runtime.
- **Budgets are opt-in and default to unbounded.** Without `--max-depth`/
  `--max-nodes`/`--max-edges`/`--time-limit-ms`, traversal continues across
  every registered component and library edge until a cycle, a reused
  context, or an external symbol provides a semantic stop - there is no
  silent default cap. When a budget does stop traversal, the run's
  `truncation_reason` and one or more `callgraph_run_frontier` rows record
  exactly where.
- **Most catalog/import mutations conservatively invalidate every entry**
  in the paired facts store; only a subsequent full extraction republishes
  them. Library-only extraction, or a zero-match write, also clears caller
  entries - extract every desired source file together to republish them
  all. Listing and dry-run catalog commands never invalidate.
- **A `CallGraphEdge.kind_id` can only ever be `1` (`calls`) or `18`
  (`dispatch_calls`)** when read through the Python SDK's call-graph-run
  reader; any other persisted relation kind in a `callgraph_run_edge` row
  raises `E_SCHEMA`. A run can never surface `uses`/`construct_value`/etc.
  edges even if a future native writer started persisting them under the
  same tables.

## Platform notes

- SQLite is vendored (amalgamation, statically linked) specifically so
  behavior is identical across macOS and RHEL regardless of the system
  SQLite version - RHEL 9's system SQLite (3.34) lacks `RETURNING`, which
  the storage layer relies on.
- `GraphQuery.references(ref)` in the Python SDK is `O(all relation sites in
  the database)` - it loads the entire `edge` -> `sites()` view and filters
  client-side by `destination_id`. Fine for a demo database; a scaling
  caveat for large codebases.
- `pip` is not present in a `uv`-managed virtual environment (such as the
  package's own dev `.venv`); use `uv pip`/`uv run`, or a separate `uv venv
  --seed` environment, when a plain `pip` is needed.
- The Python SDK's runtime has zero third-party dependencies and supports
  Python 3.12/3.13 on macOS and Linux (including RHEL-compatible), per
  `python/docs/development.md`.
