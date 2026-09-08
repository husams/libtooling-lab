# Workflow: Impact Analysis and Refactoring

## Goal

You're about to change a method's signature or behavior and need to know
the blast radius before you touch it: who calls it directly, which sibling
overrides share its interface contract, and where its type is used
elsewhere in the codebase. You also want a reliable way to tell real dead
code from a virtual method that only looks unused.

## Prerequisites

- A paired project database and facts database already extracted (see
  [01-onboarding-a-codebase](01-onboarding-a-codebase.md)).
- An open `CodeBase` in Python, referred to as `cb` below (see
  [05-python-sdk/01-getting-started](../05-python-sdk/01-getting-started.md)).

## Steps

### 1. Check direct callers of a concrete function first

For an ordinary, non-virtual function, the direct `calls` relation is
enough to establish liveness:

```python
rows = cb.query("facts::commands::detail::factsSchemaVersion") \
         .relation("calls", inbound=True) \
         .select(("qualified_name", "file", "line")).run().rows
```

```text
3 rows
```

The `select(...)` is not decoration. Without it the result is node-shaped
and its `.rows` is empty, so a caller reading `.rows` sees zero matches on
a function that has three.

The typed API gives the same evidence with names attached:

```python
fn = cb.get("facts::commands::detail::factsSchemaVersion")
print(fn.qualified_name, fn.file, fn.line)
for c in fn.callers(max_depth=1):
    print(c.qualified_name)
```

```text
facts::commands::detail::factsSchemaVersion  .../src/commands/FactPairValidationInternal.h 27
facts::commands::(anonymous namespace)::validate
facts::commands::validateFactPairForRead
facts::commands::legacyFactsNeedRegistration
```

**What this tells you:** three direct callers is unambiguous evidence this
function is live. This is the easy case; the next steps cover why the same
recipe is not enough for a virtual method.

### 2. Querying a virtual override directly can look like dead code, but isn't

```python
NAME = "facts::(anonymous namespace)::StoredCompilationDatabase::getCompileCommands"
FIELDS = ("qualified_name", "file", "line")
cb.query(NAME).relation("calls", inbound=True).select(FIELDS).run().rows            # ()
cb.query(NAME).relation("dispatch_calls", inbound=True).select(FIELDS).run().rows   # ()
```

Both come back empty, and this time the emptiness is real, not an artifact
of a missing `select(...)`. Taken at face value, this override looks
unused.

**What this tells you:** virtual dispatch edges (`dispatch_calls`, and the
`overrides` relation that identifies the interface) are recorded against
the **base declaration's canonical identity**, not against each concrete
override. Querying the override itself for callers or dispatch-callers is
the wrong question.

### 3. Resolve the override's base declaration via `overrides`

```python
cb.query(NAME).relation("overrides").select(("qualified_name", "file", "line")).run()
```

```text
clang::tooling::CompilationDatabase::getCompileCommands   (the base virtual)
```

**What this tells you:** `overrides` (relation 6, "overriding to overridden
method") points from the concrete override back to the interface it
implements. This base identity, not the override, is what call-graph
evidence is recorded against.

### 4. Query `dispatch_calls` inbound on the base for the real caller list

```python
base = "clang::tooling::CompilationDatabase::getCompileCommands"
cb.query(base).relation("dispatch_calls", inbound=True).select(("qualified_name", "file", "line")).run()
```

```text
facts::(anonymous namespace)::appendSelectedSource   src/tooling/CompilationFiles.cpp:123
facts::(anonymous namespace)::selectCommands         src/tooling/ProjectImport.cpp:317
```

**What this tells you:** the override from step 2 is not dead. Two real
call sites reach it through the base interface, dispatched virtually. They
never show up as direct callers of the override because the call in source
is written against the base type.

### 5. List every sibling override, all affected by a signature change

A change to the base virtual's signature affects every implementation of
it, whether or not that implementation shows up as a direct call target.
Resolve them the same way, this time with `inbound=True` on `overrides`
from the base:

```python
base = cb.get("clang::tooling::CompilationDatabase::getCompileCommands")
overriders = cb.query(base.qualified_name).relation("overrides", inbound=True) \
                .select(("qualified_name", "file", "line")).run()
```

```text
4 rows: PlatformCompilationDatabase, StoredCompilationDatabase,
        SharedCompilationView, RecoveryCompilation
```

[Problem Investigation](04-problem-investigation.md#2-every-overrider-of-a-virtual-query-the-base-declaration)
lists the same four rows with their files and lines. Resolve the base by
its exact qualified name (or USR) rather than the unqualified spelling
`getCompileCommands`: that spelling matches five declarations, the four
overrides plus the Clang header's base, and nothing reports the collision.
`cb.get` silently takes the first persisted match and a fluent query runs
against all five.

**What this tells you:** the full impact-analysis recipe for a virtual
method is (1) resolve the exact base declaration via `overrides` on the
changed override, (2) query `dispatch_calls` inbound on that base for real
call sites, and (3) query `overrides` inbound on that same base for every
sibling implementation. Every one of them shares the contract and is
affected by a signature change even though most of them show up in neither
of the first two queries.

### 6. Widen the blast radius from the method to its type

A signature change often changes a type, and every declaration written in
terms of that type is in scope for the same review. The relations that
answer this are `of_type`, `return_type`, and `param_type`, queried
**inbound** from the type's own symbol. `uses` (relation 7, "referencing to
referenced symbol") does not cover type-position uses at all and returns
zero rows rather than an error, which reads as "nobody uses this type" if
you stop there.

[Problem Investigation](04-problem-investigation.md#3-where-is-a-type-used-of_typereturn_type-not-uses)
works all three queries through for `facts::storage::Database`. For impact
purposes, read the union of them as the declaration set to review:
field declarations of that type, functions returning it, and parameters
taking it, including compiler-generated members such as assignment
operators.

### 7. Treat a "0 callers" sweep as a first pass, never a final verdict

A bulk sweep over `nodes(eq("kind", "function"))` for zero direct callers
is a legitimate way to generate dead-code candidates. But per step 2, every
candidate with `is_override=True` (or any interface-boundary function
called only through a base-class pointer or a Clang callback) must be
re-checked through its `overrides` base identity, exactly as above, before
being reported as dead. A full-codebase sweep like this also inherits the
per-entity navigation cost from
[02-architecture-analysis](02-architecture-analysis.md): typed navigation
such as `cb.get(x).methods()`/`.bases()` runs at roughly three seconds per
class on a real codebase, so a sweep like this does not scale past a few
dozen hand-picked candidates without narrowing the starting set first (for
example with a file-path glob).

## Pitfalls

- **Virtual dispatch edges are recorded against the base declaration's
  identity, not each override.** Querying a concrete override directly for
  callers or dispatch-callers looks like dead code even when it is actively
  used through the interface.
- **An unqualified name for a widely-overridden virtual is ambiguous.**
  Resolve the exact qualified name or USR before querying `overrides`,
  rather than letting resolution pick a match silently.
- **`uses` is not "type usage."** For "where is type T used," query
  `of_type`/`return_type`/`param_type` inbound from T; `uses` returns 0
  rows for type-position questions instead of raising an error.
- **Per-entity typed navigation is slow at scale** (roughly three seconds
  per class observed on a real codebase). Scope any full-codebase sweep to
  a bounded candidate list before iterating it.

## Where to go next

- [Custom Matchers](06-custom-matchers.md) for capturing a fact the stored
  relations above don't cover (for example, a specific expression shape)
  directly with `facts-tool match`.
- [Relations and Graph Queries](../05-python-sdk/05-relations-and-graph-queries.md)
  for the full `GraphQuery`/`EntityQuery` API used throughout this chapter.
- [Recovery and Boundaries](../04-call-graphs/04-recovery-and-boundaries.md)
  for why `DispatchCalls` is a conservative over-approximation, which
  matters when reading dispatch-based impact evidence.
- [Large Codebases and Batching](07-large-codebases-and-batching.md) for
  running the same kind of sweep across a codebase too large for one
  interactive session.
