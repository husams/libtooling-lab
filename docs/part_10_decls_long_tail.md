# Part 10 — Declarations VIII: The Long Tail, Complete

[← Part 9](part_9_decls_enums_scopes.md) | [Part 11 — Statements: Core & Selection →](part_11_stmts_core.md)

Forty-two declaration kinds remain — file-scope oddities, outlined bodies,
compiler-manufactured constants, pragmas, and the ObjC/OpenMP/OpenACC/HLSL
dialects. None deserves a part of its own; all deserve a row, because "I
can identify every line of a dump" is the promise of this deep dive. The
attribute system closes the part, since attributes decorate every
declaration you've met since Part 3.

## 10.1 — The file-scope crowd

A verified dump of five of them in nine lines of source:

```cpp
static_assert(sizeof(int) == 4, "int");
extern "C" { void cfunc(void); }
;
asm("nop");
void f() { goto out; out: return; }
```

```
StaticAssertDecl …
LinkageSpecDecl … C
EmptyDecl …
FileScopeAsmDecl …
(inside f)  LabelDecl … out
```

| Kind | Source | Key accessor / fact |
|------|--------|---------------------|
| `TranslationUnitDecl` | the root (Part 2) | `getASTContext()`; every chain of `getDeclContext()` ends here |
| `StaticAssertDecl` | `static_assert(cond, "msg");` | `getAssertExpr()`, `getMessage()`; lives at any scope |
| `LinkageSpecDecl` | `extern "C" { … }` | a `DeclContext` wrapping its contents; `getLanguage()` |
| `ExternCContextDecl` | none — an implicit context collecting `extern "C"` names TU-wide | you'll meet it only via `getDeclContext()` chains |
| `EmptyDecl` | a stray `;` at namespace scope | proof that even nothing is a node |
| `FileScopeAsmDecl` | `asm("nop");` at file scope | `getAsmString()` |
| `ExportDecl` | `export …` (C++20 modules) | a `DeclContext` like `LinkageSpecDecl` |
| `ImportDecl` | `@import` / module imports | `getImportedModule()` |
| `TopLevelStmtDecl` | statements at file scope in *incremental* mode (clang-repl) | never in a normal TU |

## 10.2 — Labels & outlined bodies

| Kind | Source | Key fact |
|------|--------|----------|
| `LabelDecl` | `out:` in `goto out;` | the *declaration* of the label; the statement side is `LabelStmt` (Part 11) — `getStmt()` links them |
| `BlockDecl` | Apple blocks `^{ … }` under `-fblocks` | verified: `BlockExpr` wraps a `BlockDecl` — a lambda-like closure with `captures()`, parameters, body |
| `CapturedDecl` | compiler-outlined regions (`#pragma omp parallel`, …) | verified under `-fopenmp`: holds the outlined body plus the `ImplicitParamDecl`s from Part 4 §4.5 |
| `OutlinedFunctionDecl` | newer outlining machinery (SYCL kernel bodies etc.) | same shape as `CapturedDecl`; recognize, move on |

The pattern to internalize: whenever a pragma or language extension needs
"this code becomes a separate function", the AST grows one of these
container decls with implicit parameters — your visitor walks *into* them
like any other `DeclContext`, which is either exactly what you want or a
source of double-counted statements, depending on the tool.

## 10.3 — Compiler-manufactured constants

Four kinds exist so that *values* the compiler must materialize have a
declaration to hang identity on:

| Kind | Produced by | Key fact |
|------|-------------|------------------|
| `TemplateParamObjectDecl` | C++20 class-type NTTP: `A<S{1}> x;` | dump shows `TemplateParamObject 'const S'` as the referenced entity of the argument — uses of `V` inside the instantiation point here |
| `LifetimeExtendedTemporaryDecl` | `const Q &q = Q{3};` at static storage | hides behind `MaterializeTemporaryExpr … extended by Var 'q'` (verified) — reachable via the expr, rarely printed itself |
| `MSGuidDecl` | `__uuidof(X)` under `-fms-extensions` | verified: the `'const _GUID'` entity behind `CXXUuidofExpr`; `getParts()` |
| `UnnamedGlobalConstantDecl` | rare constant aggregates the ABI must emit | same idea as `MSGuidDecl`, generalized |

None of these is *written*; all can be *referenced* — the reason they're
declarations at all. If your tool maps "every `DeclRefExpr` target must
have a source location", this table is the counterexample list
(`getLocation()` is invalid on most of them — Part 31's
`Loc.isInvalid()` guard).

## 10.4 — Pragma droppings

| Kind | Source |
|------|--------|
| `PragmaCommentDecl` | `#pragma comment(lib, "foo")` |
| `PragmaDetectMismatchDecl` | `#pragma detect_mismatch("name", "value")` |

Both are MSVC-compat bookkeeping: the pragma's payload, preserved as a
declaration so it survives serialization (PCH/modules). Dumps of
Windows-targeting code show them at file scope; everyone else sees them
never.

## 10.5 — The dialect tables: ObjC, OpenMP, OpenACC, HLSL

Out of scope for a C++ lab — but a dump of real-world code (any Apple SDK
header; any `-fopenmp` TU) *will* contain them, so here is the complete
roster, once, for recognition:

**Objective-C (12 kinds).** `ObjCInterfaceDecl` (`@interface`),
`ObjCImplementationDecl` (`@implementation`), `ObjCProtocolDecl`
(`@protocol`), `ObjCCategoryDecl` / `ObjCCategoryImplDecl` (categories and
their impls), `ObjCMethodDecl` (one per `-`/`+` method — the ObjC analogue
of `FunctionDecl`), `ObjCPropertyDecl` (`@property`),
`ObjCPropertyImplDecl` (`@synthesize`/`@dynamic`), `ObjCIvarDecl` (a
`FieldDecl` subclass for instance variables), `ObjCAtDefsFieldDecl`
(`@defs` relic), `ObjCCompatibleAliasDecl` (`@compatibility_alias`), and
`ObjCTypeParamDecl` (lightweight generics' `<T>` — a `TypedefNameDecl`
subclass).

**OpenMP (7 kinds).** `OMPThreadPrivateDecl`, `OMPAllocateDecl`,
`OMPRequiresDecl`, `OMPGroupPrivateDecl` — directive payloads at file
scope; `OMPDeclareReductionDecl` and `OMPDeclareMapperDecl` — the
`declare reduction`/`declare mapper` definitions; `OMPCapturedExprDecl` —
a `VarDecl` subclass for expressions captured by directive clauses.

**OpenACC (2 kinds).** `OpenACCDeclareDecl` and `OpenACCRoutineDecl` —
the `#pragma acc declare` / `acc routine` markers.

**HLSL (2 kinds).** `HLSLBufferDecl` (`cbuffer`/`tbuffer`) and
`HLSLRootSignatureDecl` — shader-land; listed so the count reaches 91,
never seen outside `-x hlsl`.

With those, all 91 concrete kinds from `DeclNodes.inc` have a home in
Parts 3–10 — `node_index.md` is the flat lookup if you ever meet a dump
line this deep dive didn't prepare you for.

## 10.6 — Attributes: the other hierarchy on declarations

Attributes — `[[nodiscard]]`, `__attribute__((visibility(…)))`,
`override`, `final` — live in their own hierarchy (`Attr`, ~400 generated
subclasses) and hang off declarations rather than being declarations:

```cpp
D->hasAttrs();
for (const Attr *A : D->attrs())
  A->getSpelling();                       // "nodiscard", "override", …
D->hasAttr<OverrideAttr>();               // the typed accessors you'll use
D->getAttr<DeprecatedAttr>();             // null if absent
```

In dumps they appear as child nodes; the sample's `Circle::area` shows the
one this lab keeps returning to:

```
CXXMethodDecl … used area 'double () const'
└─OverrideAttr <col:23>
```

That's `override` — contextual keyword in the grammar, attribute node in
the AST (Part 5 §5.2 built the missing-override check on exactly this).
Attributes carry their own arguments and source ranges, so a rewriting
tool can locate and edit them precisely; and note that some, like
`LifetimeBoundAttr` sprinkled through libc++, arrive from *system headers
with no user spelling at all* — one more reason conclusions about "what
the programmer wrote" filter through `isImplicit()`/locations first.

**Quiz.** Grand tour: a dump line reads `CapturedDecl <<invalid sloc>>
nothrow`, with three `ImplicitParamDecl` children. Without any other
context — what language feature is in play, why is the location invalid,
and which two Parts covered the pieces?

> [!hint]- Hint
> §10.2's table, plus the hidden-parameters section of the variables part.

> [!success]- Answer
> A compiler-outlined region — most likely an OpenMP directive
> (`#pragma omp parallel`) whose body was packaged into a synthetic
> function. The location is invalid because no source text declares this
> function; it's manufactured (§10.3's rule about manufactured entities
> generalizes). The container is `CapturedDecl` (§10.2), its hidden
> parameters are `ImplicitParamDecl`s (Part 4 §4.5).
