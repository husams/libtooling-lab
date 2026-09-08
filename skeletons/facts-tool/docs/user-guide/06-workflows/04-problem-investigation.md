# Workflow: Problem Investigation

## Goal

Use the Python SDK to answer the questions that come up while debugging or
reviewing a change: what actually calls this function, all the way up to
an entry point; every overrider of a virtual method; where a type is used;
and the exact source location to open. Also know, honestly, what
`facts-tool` cannot yet tell you about field writes on the current release.

This continues against the same `facts-tool`-self-analysis
`project.sqlite`/`facts.sqlite` pair used in
[Onboarding a Codebase](01-onboarding-a-codebase.md) and
[Architecture Analysis](02-architecture-analysis.md).

## Prerequisites

- An extracted `facts.sqlite`/`project.sqlite` pair.
- The Python SDK installed and `cb` an open `CodeBase`.

## Steps

### 1. Trace a real function's callers up to entry points

```python
fn = cb.get("facts::commands::detail::factsSchemaVersion")
print(fn.qualified_name, fn.kind, fn.file, fn.line)
# ... .../src/commands/FactPairValidationInternal.h 27
for c in fn.callers(max_depth=1): ...
# facts::commands::(anonymous namespace)::validate
# facts::commands::validateFactPairForRead
# facts::commands::legacyFactsNeedRegistration
for c in fn.callers(max_depth=4): ...   # climbs to
# facts::commands::(anonymous namespace)::import(...)::<lambda@188:17>::operator()
# facts::cli::dispatch(Command)::<lambda@161:7>::operator()
```

This ran in about 3.8 seconds. `factsSchemaVersion` is the exact function
whose schema-version mismatch can cause an `import` failure like the one
in [Onboarding a Codebase](01-onboarding-a-codebase.md); tracing its
callers here shows every real call path that would hit that check: import,
extract's invalidation path, `analyse dependency`, `match`, and catalog
mutations.

**What this tells you:** `fn.callers(max_depth=N)` is live relation
navigation, one hop at a time up to `N`, and climbing to `max_depth=4`
reaches all the way to the CLI's dispatch lambda in a handful of hops on a
real codebase.

### 2. Every overrider of a virtual: query the *base* declaration

```python
base = cb.get("clang::tooling::CompilationDatabase::getCompileCommands")
overriders = cb.query(base.qualified_name).relation("overrides", inbound=True) \
                .select(("qualified_name","file","line")).run()
```

```text
facts::(anonymous namespace)::PlatformCompilationDatabase::getCompileCommands   src/platform/PlatformFlags.cpp:35
facts::(anonymous namespace)::StoredCompilationDatabase::getCompileCommands     src/tooling/StoredCompilationDatabase.cpp:21
facts::commands::SharedCompilationView::getCompileCommands                     src/commands/CompilationViews.h:11
facts::commands::RecoveryCompilation::getCompileCommands                       src/commands/analyse/RecoveryCompilation.h:17
```

The unqualified spelling `getCompileCommands` matches five declarations
here: the four project overrides plus the Clang header's base. That
ambiguity is **not** reported as an error. `cb.get("getCompileCommands")`
silently returns the first persisted match (here
`PlatformCompilationDatabase::getCompileCommands`), and
`cb.query("getCompileCommands")` fans the relation query out over all five
matches at once, so the same `overrides`-inbound query returns four rows
whether you asked about the base or not.

**What this tells you:** resolve to an exact qualified name or USR before
querying relations on a virtual method. An unqualified name can match
several declarations at once, nothing warns you when it does, and the
typed and raw APIs handle the collision differently.

The four overrides above are also the full set of implementations affected
by a signature change on the base. See
[Impact Analysis and Refactoring](05-impact-analysis-and-refactoring.md)
for the rest of that recipe, including where the virtual call sites are
recorded.

### 3. Where is a type used: `of_type`/`return_type`, not `uses`

Ask for the fields you want with `select(...)`. A relation query without a
`select(...)` produces a node-shaped result whose `.rows` is empty, which
is indistinguishable from "no matches" if you only look at `.rows`:

```python
cb.query("facts::storage::Database").relation("uses", inbound=True).select(("name","file","line")).run()
# 0 rows  (a class-type target has no incoming 'uses' edges: 'uses' is about
#          referencing an existing declaration's value, not declaring a variable of that type)
cb.query("facts::storage::Database").relation("of_type", inbound=True).select(("name","file","line")).run()
# database_   src/storage/FileDatabase.h:69
# database_   src/storage/SqliteDatabase.h:316
# database_   src/storage/Storage.h:233
cb.query("facts::storage::Database").relation("return_type", inbound=True).select(("name","file","line")).run()
# openWritableFileDatabase   src/storage/FileDatabase.cpp:108
# openDatabase               src/storage/Storage.cpp:15
# operator=                  src/storage/SqliteDatabase.h:200
# operator=                  src/storage/SqliteDatabase.h:202
```

**What this tells you:** to answer "where is type T used", query
`of_type`/`return_type`/`param_type` *inbound* from the type's symbol. The
generic `uses` relation does not cover type-position uses at all, and it
silently returns zero rows instead of an error, which can look like "type
T is unused" when it is actually just the wrong relation. Note that
`return_type` includes compiler-relevant results you might not have had in
mind, such as the two assignment operators returning `Database&`.

### 4. Exact source location to open

```console
$ facts-tool symbol show s025_leaf --facts mini/facts.db --conf mini/project.db
s025_leaf() -> int
  source     s025_workflow.cpp:1:5
             /Users/.../s025_workflow.cpp
  usr        c:@F@s025_leaf#
```

File, line, and column come straight from `symbol show`, or from the
typed `.file`/`.line` attributes on an `Entity` you already have from an
SDK query.

### 5. Field-write locations: not yet available on main

There is no `writes`/`assigns`/field-mutation relation in the current
relations catalog; the only field-related relation on this release is
`field_of` (field to owning record). So on main, "where is field X
written" cannot be answered from stored relations, only "where is field X
declared" or "what record owns it". Expression- and field-access evidence
that would answer this is in-flight work (Backlog stories S-030/S-031),
**not merged to main**. See
[In Flight: F-013](../07-reference/05-in-flight-f-013.md) for what that
work adds once it lands, and do not rely on it until it does.

## Pitfalls

- **`uses` is not "type usage".** For "where is type T used", query
  `of_type`/`return_type`/`param_type` inbound from T. `uses` silently
  returns 0 rows for type-position questions instead of erroring, which
  can be mistaken for "unused".
- **An unqualified name can resolve ambiguously, and nothing tells you
  so.** `cb.get(ref)` takes the first persisted match; a fluent query on
  the same spelling runs against every match. Resolve to an exact
  qualified name or USR first (`symbol find`/`symbol show`, or a
  `select`ed query filtered to the exact `qualified_name`) before running
  a relation query you intend to trust for one specific declaration,
  especially for virtual methods with a base and several overrides.
- **A relation query with no `select(...)` has an empty `.rows`.** The
  result is node-shaped, so reading `.rows` on it reports zero matches
  even when the relation has plenty. Add `select(...)` whenever you plan
  to read rows.
- **Field-write locations are not available on main.** Don't promise this
  capability to a reader or a script; point to
  [In Flight: F-013](../07-reference/05-in-flight-f-013.md) instead of
  guessing at a relation name that doesn't exist yet.

## Where to go next

- [Query Model](../05-python-sdk/03-query-model.md) for predicate and
  relation syntax used above.
- [Relations and Graph Queries](../05-python-sdk/05-relations-and-graph-queries.md)
  for `relation(..., inbound=True)` and typed navigation in depth.
- [In Flight: F-013](../07-reference/05-in-flight-f-013.md) for the
  expression/field-access evidence work that is not yet on main.
- [Impact Analysis and Refactoring](05-impact-analysis-and-refactoring.md)
  to take the base/override resolution from step 2 further, into "what
  breaks if I change this signature".
