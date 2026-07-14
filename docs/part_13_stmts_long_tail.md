# Part 13 — Statements: The Long Tail

[← Part 12 — Loops & Jumps](part_12_stmts_loops_jumps.md) | [Part 14 — Expressions: The Expr Base →](part_14_expr_base.md)

This part completes the statement inventory: exceptions, inline assembly,
coroutine machinery, outlined regions, and the three pragma-based families
(OpenMP, OpenACC, Objective-C's `@`-statements) that together make up the
majority of `StmtNodes.inc` by count. The C++-relevant kinds get sections;
the out-of-scope families get *complete* name tables — every kind listed, so
you can always identify what a dump shows you, even when this lab doesn't
teach it.

## 13.1 — CXXTryStmt and CXXCatchStmt

`CXXTryStmt` owns the guarded block and its ordered handlers, each a
`CXXCatchStmt`. Verified shape:

```
CXXTryStmt
├─CompoundStmt                          ← getTryBlock()
├─CXXCatchStmt                          ← getHandler(0)
│ ├─VarDecl e 'const int &'             ← getExceptionDecl()
│ └─CompoundStmt
└─CXXCatchStmt                          ← catch (...)
  └─CompoundStmt                        ← exception decl is NULL here
```

```cpp
TS->getTryBlock();
TS->getNumHandlers();  TS->getHandler(i);      // in source order
CS->getExceptionDecl();   // VarDecl* — NULL for catch (...)
CS->getCaughtType();      // QualType — invalid for catch (...)
CS->getHandlerBlock();
```

Handler *order* is semantic — first match wins — so checks like "catch by
reference" or "handler for a base class shadows a derived one" iterate
`getHandler(i)` in order and compare caught types with the Part 25–29
type APIs. The throwing side (`CXXThrowExpr`) is an expression and lives in
Part 21.

## 13.2 — SEH: the Windows exception statements

Structured Exception Handling is MSVC's pre-C++-exceptions mechanism, alive
in Windows codebases and fully represented in the AST:

| Node | Source shape | Key accessors |
|------|--------------|---------------|
| `SEHTryStmt` | `__try { … }` | `getTryBlock()`, `getHandler()` (except **or** finally, never both) |
| `SEHExceptStmt` | `__except (filter) { … }` | `getFilterExpr()` — evaluated at *throw* time, `getBlock()` |
| `SEHFinallyStmt` | `__finally { … }` | `getBlock()` |
| `SEHLeaveStmt` | `__leave;` | leaf; jumps to the end of the enclosing `__try` |

On macOS targets you'll only produce these with `-fms-extensions` and a
Windows triple; recognize them so Windows-origin dumps aren't a mystery.

## 13.3 — Inline assembly: GCCAsmStmt and MSAsmStmt

Both derive from the abstract `AsmStmt`, and both are best treated by tools
as opaque barriers with declared inputs/outputs:

```cpp
AS->getAsmString();       // the template text
AS->getNumOutputs(); AS->getOutputExpr(i); AS->getOutputConstraint(i);
AS->getNumInputs();  AS->getInputExpr(i);  AS->getInputConstraint(i);
AS->getNumClobbers(); AS->getClobber(i);
```

`GCCAsmStmt` is the `asm volatile ("…" : outs : ins : clobbers)` dialect —
including `asm goto`, whose possible jump targets (`getNumLabels()`,
`getLabelExpr(i)`) make it the third kind of goto in the language.
`MSAsmStmt` is MSVC's `__asm { mov eax, x }` block dialect. For analysis
purposes: the constraint expressions are real `Expr` children (visitors do
walk into them), the assembly text is not parsed into the AST.

## 13.4 — Coroutine statements

A coroutine's body is wrapped, range-for-style, in machinery — but an order
of magnitude more of it. **`CoroutineBodyStmt`** replaces the plain
`CompoundStmt` body of any function containing `co_await` / `co_yield` /
`co_return`, and its children include the user's body plus the synthesized
promise calls:

```cpp
CBS->getBody();               // what the user wrote
CBS->getPromiseDeclStmt();    // DeclStmt for __promise
CBS->getInitSuspendStmt();    // co_await promise.initial_suspend()
CBS->getFinalSuspendStmt();   // co_await promise.final_suspend()
CBS->getReturnStmt();         // the get_return_object() plumbing
CBS->getFallthroughHandler(); CBS->getExceptionHandler();  // …and more
```

**`CoreturnStmt`** is `co_return e;` — `getOperand()` is the value,
`getPromiseCall()` the resolved `promise.return_value(e)` /
`return_void()` call. The companion *expressions* (`CoawaitExpr`,
`CoyieldExpr`, `DependentCoawaitExpr`) are in Part 24. The practical
takeaway mirrors range-for, scaled up: one keyword in source, a dozen
resolved calls in the tree, all of them real nodes your visitor will walk
into unless it checks `isImplicit()` / compares against `getBody()`.

## 13.5 — Outlined and dialect-specific statements

Five kinds that don't fit any family above, completing the C/C++-side
inventory:

| Node | What it is | Notes |
|------|------------|-------|
| `CapturedStmt` | a region outlined into a synthetic function | the body of every OpenMP directive; `getCapturedDecl()` → Part 10's `CapturedDecl`, `captures()` lists captured variables |
| `OMPCanonicalLoop` | normalized-loop wrapper inside OpenMP loop directives | wraps a `ForStmt`/`CXXForRangeStmt` that satisfies OpenMP's canonical-form rules |
| `MSDependentExistsStmt` | MSVC `__if_exists(name) { … }` inside templates | dependent: resolved at instantiation |
| `DeferStmt` | C2y `_Defer { … }` (Clang extension) | runs its substatement at scope exit — C's answer to destructors |
| `SYCLKernelCallStmt` | SYCL kernel-invocation wrapper | only under `-fsycl`; carries the outlined kernel entry point |

## 13.6 — Objective-C statements (complete table)

Out of scope for a C++ lab, but here is the *entire* family, so nothing in
a mixed-language dump is ever unidentifiable:

| Node | Source shape |
|------|--------------|
| `ObjCAtTryStmt` | `@try { … }` |
| `ObjCAtCatchStmt` | `@catch (E *e) { … }` |
| `ObjCAtFinallyStmt` | `@finally { … }` |
| `ObjCAtThrowStmt` | `@throw e;` |
| `ObjCAtSynchronizedStmt` | `@synchronized (obj) { … }` |
| `ObjCAutoreleasePoolStmt` | `@autoreleasepool { … }` |
| `ObjCForCollectionStmt` | `for (id x in collection)` |

That's all seven — Objective-C's statement additions are small; its real
weight is on the expression side (Part 24 lists those).

## 13.7 — OpenMP directives (complete table)

`#pragma omp …` lines become statement nodes — one class per directive —
and at 77 kinds they are the largest single block in `StmtNodes.inc`. All
of them derive from `OMPExecutableDirective`, all carry their pragma's
clauses (`clauses()`) and, for most, a `CapturedStmt` body. Out of scope
for this lab; the complete inventory, grouped by what the directive does:

**Parallelism & teams:** `OMPParallelDirective`, `OMPTeamsDirective`,
`OMPTargetParallelDirective`, `OMPTargetTeamsDirective`.

**Worksharing loops:** `OMPForDirective`, `OMPForSimdDirective`,
`OMPSimdDirective`, `OMPDistributeDirective`, `OMPDistributeSimdDirective`,
`OMPDistributeParallelForDirective`,
`OMPDistributeParallelForSimdDirective`, `OMPParallelForDirective`,
`OMPParallelForSimdDirective`, `OMPTeamsDistributeDirective`,
`OMPTeamsDistributeSimdDirective`,
`OMPTeamsDistributeParallelForDirective`,
`OMPTeamsDistributeParallelForSimdDirective`.

**Generic loop construct:** `OMPGenericLoopDirective`,
`OMPParallelGenericLoopDirective`, `OMPTeamsGenericLoopDirective`,
`OMPTargetParallelGenericLoopDirective`,
`OMPTargetTeamsGenericLoopDirective`.

**Sections & single:** `OMPSectionsDirective`, `OMPSectionDirective`,
`OMPParallelSectionsDirective`, `OMPSingleDirective`, `OMPScopeDirective`.

**Master / masked:** `OMPMasterDirective`, `OMPMaskedDirective`,
`OMPParallelMasterDirective`, `OMPParallelMaskedDirective`.

**Tasking:** `OMPTaskDirective`, `OMPTaskLoopDirective`,
`OMPTaskLoopSimdDirective`, `OMPMasterTaskLoopDirective`,
`OMPMasterTaskLoopSimdDirective`, `OMPMaskedTaskLoopDirective`,
`OMPMaskedTaskLoopSimdDirective`, `OMPParallelMasterTaskLoopDirective`,
`OMPParallelMasterTaskLoopSimdDirective`,
`OMPParallelMaskedTaskLoopDirective`,
`OMPParallelMaskedTaskLoopSimdDirective`, `OMPTaskgroupDirective`,
`OMPTaskwaitDirective`, `OMPTaskyieldDirective`.

**Target offload & data:** `OMPTargetDirective`, `OMPTargetDataDirective`,
`OMPTargetEnterDataDirective`, `OMPTargetExitDataDirective`,
`OMPTargetUpdateDirective`, `OMPTargetParallelForDirective`,
`OMPTargetParallelForSimdDirective`, `OMPTargetSimdDirective`,
`OMPTargetTeamsDistributeDirective`,
`OMPTargetTeamsDistributeSimdDirective`,
`OMPTargetTeamsDistributeParallelForDirective`,
`OMPTargetTeamsDistributeParallelForSimdDirective`.

**Synchronization & memory:** `OMPBarrierDirective`, `OMPCriticalDirective`,
`OMPAtomicDirective`, `OMPFlushDirective`, `OMPOrderedDirective`,
`OMPScanDirective`, `OMPDepobjDirective`.

**Cancellation:** `OMPCancelDirective`, `OMPCancellationPointDirective`.

**Loop transformations:** `OMPTileDirective`, `OMPStripeDirective`,
`OMPUnrollDirective`, `OMPReverseDirective`, `OMPInterchangeDirective`,
`OMPFuseDirective`.

**Meta & misc:** `OMPMetaDirective`, `OMPAssumeDirective`,
`OMPErrorDirective`, `OMPDispatchDirective`, `OMPInteropDirective`.

(Count: 77 executable-directive kinds in this LLVM's `StmtNodes.inc`; the
OpenMP *expression* helpers — `OMPArrayShapingExpr`, `OMPIteratorExpr`,
`ArraySectionExpr` — are expressions and appear in Part 24's tail.)

## 13.8 — OpenACC constructs (complete table)

The newer accelerator-pragma dialect, same design as OpenMP — one statement
node per construct, 14 kinds in this LLVM:

`OpenACCComputeConstruct`, `OpenACCLoopConstruct`,
`OpenACCCombinedConstruct`, `OpenACCDataConstruct`,
`OpenACCEnterDataConstruct`, `OpenACCExitDataConstruct`,
`OpenACCHostDataConstruct`, `OpenACCInitConstruct`,
`OpenACCShutdownConstruct`, `OpenACCSetConstruct`,
`OpenACCUpdateConstruct`, `OpenACCWaitConstruct`,
`OpenACCCacheConstruct`, `OpenACCAtomicConstruct`.

(The related `OpenACCAsteriskSizeExpr` is an expression — Part 24.)

**Quiz.** Your visitor's `VisitStmt` count for a function jumps from 40 to
400 after someone adds one `#pragma omp parallel for` above its loop. Name
the two nodes from this part that appear between the directive and the
user's loop, and say why the count explodes.

> [!hint]- Hint
> §13.5 has both of them, and one of them means "this code was outlined
> into a synthetic function — captures and all".

> [!success]- Answer
> The `OMPParallelForDirective` owns a `CapturedStmt` (the outlined region,
> with a `CapturedDecl` and capture records for every variable used), which
> wraps an `OMPCanonicalLoop` around the original `ForStmt`. The explosion
> comes from the capture machinery plus the directive's clause expressions —
> all real nodes, all visited, none written by the user.
