# Concepts and glossary

This chapter introduces the vocabulary the rest of the guide assumes.
Read it once before the extraction and call-graph chapters - several terms
here (freshness, coverage, completeness, pairing) look like synonyms but are
deliberately distinct axes of "how much can I trust this data."

## Facts vs. project: two databases, one pair

`facts-tool` always works against two SQLite databases together, never one
alone:

- The **facts database** stores symbols, relations between symbols,
  relation *sites* (the exact source location of one occurrence of a
  relation), and include-dependency facts. This is what `extract`,
  `match`, and `analyse` write into.
- The **project database** (also called the project/configuration database
  or the catalog) stores repositories, the checkout clones registered under
  them, project components, indexed directories, registered files, and the
  compile configuration for each file. This is what `repo`, `component`,
  `dir`, `file`, and `import` manage. It also owns `FileId` values.

A **pair** or **paired store** is a facts database and a project database
opened together, where every `FileId` referenced by a fact in the facts
database must resolve against the project database's file table.

**Pairing** is whether the tooling can prove two databases came from the
same indexing run. Numeric `FileId` overlap alone is never sufficient
proof - a successful open can still report pairing as `unverifiable`.
Newer facts databases (schema 11+) carry a `facts_project_provenance` table
as stronger identity evidence that the native writer uses to reject
obviously incompatible pairs going forward, but migrating an *old* facts
database up to schema 11+ does not retroactively establish that stronger
provenance for data extracted before the migration. If the native writer
reports that pairing cannot be proved, the safe response is to write to a
new facts file and re-extract every source, not to trust the existing pair.

## Freshness, coverage, and completeness are three different things

These three concepts are easy to conflate but describe unrelated
guarantees. Keep them separate:

1. **Extraction coverage** - was a given translation unit actually
   extracted at all, and does a given symbol have committed call evidence
   from that extraction? This is what a *function entry* (see below)
   records.
2. **Source freshness** - does a stored fact still match the file on disk
   right now? This is largely *unknown* unless something has explicitly
   re-validated it (for example, a call-graph recovery attempt).
3. **Traversal completeness** - did one particular `analyse call-graph`
   run reach every node it could reach, or did it stop early at a budget,
   a cycle, or an external boundary? This is the run's `status`
   (`complete`, `truncated`, and so on).

A call-graph run can be `complete` (it exhausted everything reachable under
its scope and budget) while the underlying source has gone stale on disk,
or while a definition is genuinely missing from the project entirely (an
external boundary). None of coverage, freshness, or completeness implies
either of the others - a missing function entry alone never forces you to
re-extract, and a "complete" run never promises the stored facts are fresh.

## Symbols, USRs, and qualified names

A **symbol** is one row in the facts database's `symbol` table: a
declaration, definition, or other named (or synthetically named) entity
that Clang's AST visitor recorded. Its `kind`/`kind_id` is the raw integer
value of Clang's own `clang::index::SymbolKind` enum (0–31 as of LLVM 22,
covering `unknown` through `concept`); any value the tool doesn't recognize
yet renders as `kind_N`.

The **USR** (Unified Symbol Resolution) is Clang's canonical, string-form
identity for a symbol, and it is the join key `facts-tool` uses to merge a
declaration with its later definition, and to merge occurrences of the same
symbol across translation units. This matters most for template
specializations: every specialization of a class template shares one
human-readable qualified name, so only the USR can actually distinguish
`Holder<Widget>` from `Holder<int>`.

The **qualified name** is the human-facing, scoped name you'd expect to
read (`shapes::Circle::area`). For most symbols it is exactly what Clang
would print. One deliberate exception: lambda closures get a stable,
synthesized `<lambda@LINE:COLUMN>` name instead of Clang's own
version-dependent lambda spelling, so that qualified names for lambdas
don't change across compiler upgrades. This only changes the human-facing
name - the USR identity of a lambda is untouched.

A **compiler-provided** (or **locationless**) **callable** is a real symbol
row for an implicit Clang declaration - the canonical example is an
implicit `operator new`/`operator delete` - that has a valid, canonical USR
but zero declaration coordinates and no definition row, because it has no
physical declaration in your source at all. These live at `FileId 0`. Real
call sites that call one of these still carry the *caller's* real file,
line, and column; only the callee itself has no location.

## Relations and sites

A **relation** is a directed, typed edge between two symbols - for example
`Calls`, `Inherits`, `Overrides`, or `OfType`. There are 23 relation kinds
in total, covering calls, inheritance, containment, specialization,
instantiation, overriding, type usage, construction variants, destruction,
friendship, and dispatch. Edge rows carry the source symbol, destination
symbol, relation kind, and structural metadata like access level or
whether a base is virtual.

A **site** (or **relation site**) is a separate, finer-grained record of
one concrete source occurrence of an edge - its file, line, column, offset,
and for calls, the receiver's type and a certainty value. One edge can have
multiple sites: if a function calls the same callee twice, that's one
`Calls` edge with two sites.

**Certainty** on a call site is either `exact` (a concrete by-value
receiver, so the callee is proven) or `possible` (a pointer, reference,
implicit, or otherwise unproven receiver). There is no persisted `unknown`
certainty value - a call site is always classified as one or the other.

## Call graphs: roots, runs, frontiers, boundaries, recovery

A **root** is the starting symbol (or symbols) for a call-graph traversal,
selected either by exact qualified name or USR, or by requesting every
definition-backed symbol in the project.

A **run** (or **call-graph run**) is one complete, append-only, persisted
record of a single `analyse call-graph` invocation, identified by a
`run_id`. Runs are never rewritten or deleted by a later run, so multiple
runs can coexist in the same facts database and you always know exactly
which invocation produced which edges.

A **frontier** is a discovered-but-not-admitted graph endpoint - the node a
run wanted to expand next but couldn't, because a budget or time limit
stopped it there first. Frontier rows exist only for *truncated* runs, not
for complete ones.

A **boundary** (or **external boundary**) is a call target whose
declaration is known but whose definition is not (yet) in the project. A
run that stops at a boundary is a *complete* stop, not a truncation -
`facts-tool` distinguishes "I chose to stop because there's nothing more to
find here" from "I was forced to stop by a budget."

**Recovery** (a **recovery attempt**) is the explicit, opt-in action
(`--recover-missing`) of extracting a registered-but-not-yet-indexed
translation unit into an isolated temporary store, purely to validate or
supply missing call evidence for the current run. Every attempted TU is
recorded as its own row, so you can see exactly what recovery tried and
why it succeeded or failed.

A **budget** is an explicit, opt-in traversal limit - depth, node count,
edge count, or wall-clock time. There is no silent default cap; an
unbounded traversal is the default behavior, and you have to ask for a
budget to get one.

A **function entry** is a per-function record of whether that function's
call evidence has been fully, successfully collected at least once by
`extract`. Entries are published only by `extract`, never by `match` or
`analyse call-graph` - those commands read existing evidence, they don't
create new entries.

## Configuration and catalog vocabulary

The **matched-symbol index** (or match-only index) is a table in the
project database populated *only* by successful `match` invocations, never
by `extract`. A miss in this index never proves a symbol doesn't exist -
it only proves nothing has matched it yet.

A **page** (or **paging**) is bounded, cursor-based enumeration of a result
or child collection, so no query can silently return an unbounded set.

**Provenance** is the recorded canonical file identity evidence a facts
store uses to prove - or fail to prove - that it agrees with a paired
project database about what a given `FileId` means.

A **witness** (or **path witness**) is one concrete, node-simple path
returned by a path query between a start and a target symbol, backed by
real stored relation-site steps - not a synthesized or approximate route.

## Full glossary

| Term | Definition |
|---|---|
| Facts database | SQLite store of symbols, side tables, relations, relation sites, and include-dependency facts |
| Project database | SQLite store of repositories, clones, components, directories, files, and compile configuration; owns FileIds |
| Pair / paired store | A facts DB + project DB opened together, with FileIds cross-checked |
| Pairing | Whether two databases can be proven to come from the same indexing run |
| Freshness | Whether a stored fact still matches on-disk source right now |
| Coverage / extraction coverage | Whether a symbol's body/calls were actually committed by a prior extraction |
| Traversal completeness | Whether a call-graph run reached every reachable node under its scope/budget |
| USR | Clang's canonical cross-TU symbol identity string |
| Qualified name | Human-facing scoped name; synthesized for lambdas |
| TU (translation unit) | One compiled source file plus its includes, as Clang sees it |
| Root | The starting symbol(s) for a call-graph traversal |
| Frontier | Discovered-but-not-admitted graph endpoints when a budget stops traversal |
| Boundary / external boundary | A call target with a known declaration but no in-project definition; a complete stop, not truncation |
| Recovery / recovery attempt | Opt-in extraction of an unindexed TU into a temporary store to validate missing call evidence |
| Run (call-graph run) | One append-only, persisted record of a single `analyse call-graph` invocation |
| Function entry | A per-function record of whether its call evidence was fully collected |
| Matched-symbol index | Project-DB table populated only by successful `match`, never by `extract` |
| Witness (path witness) | One concrete node-simple path backed by real relation-site steps |
| Budget | An explicit, opt-in traversal limit; never a silent default |
| Page / paging | Bounded, cursor-based enumeration |
| Provenance | Recorded file identity evidence used to validate a facts/project pair |
| Compiler-provided / locationless callable | An implicit Clang declaration's symbol row with a valid USR but no location |
| Site / relation site | One concrete source occurrence of a relation edge |
| Certainty | `exact` or `possible` on a call's relation site; there is no persisted `unknown` |

For the full relation-kind table (all 23 kinds with IDs) see
[07-reference/02-storage-schema.md](../07-reference/02-storage-schema.md).
For the call-graph concepts in more depth, with worked examples, see
[04-call-graphs/01-overview](../04-call-graphs/01-overview.md).

## What's next

[03-installation](03-installation.md) builds the native tool and installs
the Python SDK so you can start putting these concepts into practice.
