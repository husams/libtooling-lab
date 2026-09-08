# What Gets Extracted

`extract` records symbols, their qualifiers, and the relations between them.
This chapter describes exactly what shows up in the facts database, how
identity works across translation units, and what is deliberately left out.

## Symbol kinds

Every symbol's `kind`/`kind_id` is the raw integer value of Clang's
`clang::index::SymbolKind` (LLVM 22), 0 through 31 (`unknown` through
`concept`). A future LLVM release that adds new kind values surfaces as
`kind_N` rather than failing. The CLI renders each kind with Clang's own
hyphenated spelling (`instance-method`, `template-type-param`,
`enum-constant`), not an underscored one. A real facts database built from a
small project with virtual dispatch, templates, and a lambda produced this
distinct set of kinds:

```text
class, constructor, destructor, field, function, instance-method,
namespace, struct, template-type-param, type-alias, variable
```

The database behind that list holds 85 `symbol` rows, of which `symbol list`
prints 83: the two extra rows are builtin types (`int`, `double`) recorded at
kind `0` with synthetic `c:@BT@...` USRs, which the listing omits. That is
why an extraction reporting `83 symbol(s) recorded` and a `symbol list` of 83
entries agree with each other while `SELECT count(*) FROM symbol` returns 85.

## Qualified names

Qualified names are the human-facing, dotted/scoped spelling of a symbol.
One exception: **lambda closures get a synthesized name**, not Clang's raw
spelling. The extractor emits `<lambda@LINE:COLUMN>` for every lambda
closure type, so the qualified name is stable across Clang versions instead
of depending on Clang's own version-dependent `(lambda)`/`(anonymous
class)` spelling. This only changes the human-facing name - the symbol's USR
identity is untouched. Verified example, `symbol list` output from a real
extraction:

```text
kind                qualified name
class               <lambda@25:41>
instance-method     main()::<lambda@25:41>::operator()(double value) const -> double
constructor         main()::(lambda)::(lambda at .../main.cpp:25:41)
```

## USRs

The USR (Unified Symbol Resolution string) is Clang's canonical, cross-TU
symbol identity - the join key that lets a declaration in one file and its
out-of-line definition in another resolve to the same stored `Symbol` row.
Qualified names alone cannot do this reliably: every specialization of a
class template shares one qualified name, so only the USR distinguishes,
say, `Holder<Widget>` from `Holder<int>`. A `match` run against
`Circle::area` shows the same USR appearing twice - once for the
declaration, once for the out-of-line definition:

```console
$ facts-tool symbol find -c demo.db --name area
USR                            QUALIFIED NAME        FILE ID  KIND
c:@N@shapes@S@Circle@F@area#1  shapes::Circle::area  2        17
c:@N@shapes@S@Circle@F@area#1  shapes::Circle::area  820      17
```

The real output is tab-separated and carries three further columns (`PATH`,
`COMPONENT`, `REPOSITORY`) that are trimmed here; see
[inspecting symbols from the CLI](05-inspecting-symbols-cli.md#symbol-find)
for the full row.

## Locations

A symbol's declaration location, its definition location (if any), and each
relation site's location are three **independently tracked** file/line/
column/offset tuples - not one shared location. This matters even when the
callee has no location of its own: a real call site into an implicit,
compiler-provided `operator new` still keeps the caller's real file, line,
and column, even though the callee itself has zero declaration coordinates.

## Relations

`extract` records 23 relation kinds, each edge carrying source, destination,
kind, position, access, virtual-base, implicit, lexical, and count fields. A
separate, finer-grained `relation_site` table records every source
occurrence of an edge (file/line/column/offset, plus receiver type and
certainty for calls).

| ID | Kind | Notes |
|---|---|---|
| 1 | `calls` | statically selected callee |
| 2 | `inherits` | edge carries access + virtual-base flag |
| 3 | `contains` | - |
| 4 | `specializes` | - |
| 5 | `instantiates` | - |
| 6 | `overrides` | from a derived method to its base declaration |
| 7 | `uses` | - |
| 8 | `field_of` | - |
| 9 | `method_of` | - |
| 10-14 | `construct_value`/`temp`/`heap`/`copy`/`move` | - |
| 15 | `factory_construct` | - |
| 16 | `destroy` | - |
| 17 | `friend` | - |
| 18 | `dispatch_calls` | conservative virtual-dispatch target |
| 19 | `alias_of` | - |
| 20 | `of_type` | declaration to its declared type |
| 21 | `return_type` | - |
| 22 | `param_type` | - |
| 23 | `template_argument_type` | - |

### Calls vs. dispatch calls, and certainty

`calls` (1) is used for statically resolvable callees. `dispatch_calls` (18)
is used for virtual targets and is deliberately conservative - it may
include targets that are not actually reachable at runtime for a given
static type, rather than under-approximating. A real traversal through a
virtual `area()` call resolved to both concrete overrides:

```text
('shapes::describe', 'shapes::Circle::area', 18, 3),
('shapes::describe', 'shapes::Square::area', 18, 3)
```

Every call's `relation_site` carries a `certainty`: `exact` when a concrete
by-value receiver's type is proven, `possible` for a pointer, reference,
implicit, or otherwise unproven receiver. There is no persisted `unknown`
certainty value.

### Type-use relations

`of_type` (20), `return_type` (21), `param_type` (22), and
`template_argument_type` (23) connect a declaration to the types it uses.
Note: field read/write access (as opposed to type-use relations) is **not**
part of these 23 kinds on the current release - that evidence is new,
opt-in, and not yet on `main` (see
[the in-flight appendix](../07-reference/05-in-flight-f-013.md)).

## Callable qualifiers

Functions, methods, and function/method templates carry:

- `const`/`volatile`/`ref`-qualifiers (methods only)
- `is_noexcept` - proven non-throwing, not merely a written `noexcept`
  keyword; dependent specifications stay `false`
- `is_explicit` - same "proven, not just written" semantics
- constant-evaluation as an enum: `none | constexpr | consteval | constinit`
  (not bit flags)

A real `symbol show` for an overriding, const virtual method:

```console
$ facts-tool symbol show 'shapes::Circle::area' -c demo.db
shapes::Circle::area() const -> double
  identity   820:7
  kind       instance-method
  type       Function
  language   C++
  access     public
  source     shapes.hpp:17:10
             .../proj/include/shapes.hpp
  usr        c:@N@shapes@S@Circle@F@area#1
  properties none
  flags      definition, virtual, const, override
```

`is_volatile` only exists starting at facts schema version 10; older or
migrated rows read `0` meaning "never recorded," not "confirmed
non-volatile." Rendering order for qualifier suffixes is
const/volatile/ref/noexcept.

## Compiler-provided / locationless callables

Explicit project calls into Clang's *implicit* declarations - the
implicit `operator new`/`operator delete` family and similar compiler-
synthesized `FunctionDecl`s - get a real, valid, canonical USR-keyed
`Symbol` row with **zero declaration coordinates and no definition row**.
Real call sites into these symbols still carry the *caller's* real file,
line, and column. `FileId 0` is reserved for these locationless symbols: it
hosts fixed primitive-type IDs and dynamically-allocated compiler-symbol
IDs. The native call graph treats these the same as ordinary function rows
and still stops cleanly at external boundaries when it reaches one.

## Deliberate exclusions

**System headers are excluded from location-bearing symbol extraction** by
long-standing design policy. This mostly means what you would expect (no
`Symbol` rows for `std::` declarations themselves, only for how project code
uses them), but it has produced real, now-fixed edge cases worth knowing
about:

- A member, field, or enumerator defined out-of-line whose *owner record*
  lived in a filtered system header used to abort the entire extraction (the
  owner symbol was never persisted). This is fixed: such owners now get a
  persisted `is_external=1` stub keyed by USR, later reconciled if a real
  definition appears. References through anonymous union/struct fields
  (which have no name to build a USR from) are now correctly skipped rather
  than aborting the extraction.
- A dependent `alignof(T)` inside an uninstantiated template used to crash
  the constant evaluator. This is fixed: value/type/instantiation-dependent
  expressions are now rejected before evaluation, and the written text is
  persisted with `evaluated_kind='none'`.

## Known limitations that affect what you see

- **A class-template specialization that is named without being required
  complete** (`TSK_Undeclared` - e.g. a pointer-typed field, a
  reference-only parameter, or an alias to `Holder<Widget>` with no use that
  forces instantiation) can make relation-kind resolution fail with an
  unavailable USR. When this happens, the **entire extraction rolls back to
  zero symbols** and exits 1. A single named-but-not-instantiated template
  specialization anywhere in a translation unit can zero out an otherwise
  successful extraction of that whole file. Treat any all-or-nothing
  zero-symbol extraction result as a signal to check for this pattern before
  assuming a project-wide extraction failure.
- **`coverage.unsupported_semantics kind=implicit-cleanup` notices** (see
  [Extracting Facts](01-extract.md)) are routine noise on any translation
  unit that includes the standard library. They print at every verbosity
  level, `-v 0` included, and they do not indicate a failure.

If you hit a case that does not match this chapter, verify current behavior
against a fresh extraction before reporting it as a defect - several
historical write-ups of extraction failures in this project were already
fixed by later work.
