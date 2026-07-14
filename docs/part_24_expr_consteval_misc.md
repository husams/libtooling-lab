# Part 24 — Expressions: Constant Evaluation & the Complete Tail

[← Part 23 — Dependence & Packs](part_23_expr_dependent_packs.md) | [Part 25 — QualType & Builtins →](part_25_types_qualtype_builtins.md)

The last expressions part does two jobs: it covers the constant evaluator —
the API you'll call more than any node kind here — and then closes the books
on the expression hierarchy: every kind Clang defines that Parts 14–23
didn't own is accounted for below. After this page, the expression side of
`StmtNodes.inc` contains nothing unnamed.

## 24.1 — Constant evaluation

The AST ships with the compiler's constant evaluator attached. Any
expression can be asked to fold itself:

```cpp
Expr::EvalResult Result;
if (E->EvaluateAsInt(Result, Ctx)) {          // Ctx = ASTContext
  llvm::APSInt V = Result.Val.getInt();       // the value, arbitrary precision
}
E->isIntegerConstantExpr(Ctx);                // is it an ICE at all?
E->EvaluateAsRValue(Result, Ctx);             // non-integer folding too
E->EvaluateAsBooleanCondition(B, Ctx);
E->isValueDependent();                        // don't even try inside templates
```

The result type, **`APValue`**, is a variant covering ints, floats,
pointers-with-offsets, whole structs — everything `constexpr` can produce.
`ConstantExpr` (Part 22) caches one where the language demanded it, and
`constexpr` variables like `geo::kPi` expose theirs through
`VarDecl::evaluateValue()`.

The practical pattern — "warn if this `SQUARE(x)` argument makes the result
overflow", "is this array index provably in range" — is always the same
three steps: check `isValueDependent()`, call the right `Evaluate*`, and
treat failure as "not constant" rather than an error. Plenty of correct
code simply isn't foldable, and the evaluator is honest about it.

## 24.2 — CXXRewrittenBinaryOperator: C++20's ghost writer

With a defaulted `operator<=>`, the comparison you write is not the
comparison that runs — `a < b` executes `(a <=> b) < 0`, and `a != b`
executes `!(a == b)`. The AST records both truths in
**`CXXRewrittenBinaryOperator`** (verified: four in a two-function
`<compare>` probe):

```cpp
RBO->getOpcode();               // what you wrote: BO_LT, BO_NE …
RBO->getSemanticForm();         // what runs: the <=> call, wrapped
RBO->isReversed();              // b <=> a with arguments swapped?
```

Same design as `InitListExpr` and `PseudoObjectExpr`: a syntactic face over
a semantic body. Operator-related checks written before C++20 routinely
miss these — a `binaryOperator()` matcher does *not* match a rewritten
`!=`; you need `cxxRewrittenBinaryOperator()` alongside it.

## 24.3 — Coroutine expressions

A coroutine body (statement side: `CoroutineBodyStmt`, `CoreturnStmt`,
Part 13) contains three expression kinds — verified against a minimal
`std::coroutine_handle` task type under `-std=c++20`:

- **`CoawaitExpr`** — `co_await e`. The dump shows what the "one keyword"
  really is: children for the operand plus the three protocol calls
  (`await_ready`, `await_suspend`, `await_resume`) — a whole function-call
  conversation encoded as one expression node. Three of them appear even in
  a trivial coroutine: yours, plus the implicit awaits on
  `initial_suspend()`/`final_suspend()`.
- **`CoyieldExpr`** — `co_yield v`: sugar for
  `co_await promise.yield_value(v)`, and the node keeps both spellings
  (same syntactic/semantic duality as §24.2).
- **`DependentCoawaitExpr`** — `co_await awaitable` inside a template,
  where the awaiter protocol can't be resolved yet (verified in a templated
  coroutine probe): Part 23's "not yet" discipline extended to coroutines.

## 24.4 — _Generic

**`GenericSelectionExpr`** — C's `_Generic(x, int: a, default: b)`
(verified in a C probe; Clang also accepts it in C++ as an extension). The
node keeps *all* branches plus the index of the chosen one
(`getResultExpr()`, `isResultDependent()`). Its niche in C++ tooling:
cross-language headers use it inside macros, so C++ tools do meet it —
usually via `-ast-dump` archaeology of a macro expansion (Part 31's
spelling-vs-expansion skills apply verbatim).

## 24.5 — The complete remaining tail

Everything below exists, is parsed into real nodes, and is out of scope for
a C++-on-macOS lab — listed so that the inventory is *complete*, with the
one accessor or fact worth knowing. First the Objective-C expression family
(these appear the moment a TU is `.m`/`.mm`):

| Node | Produces it |
|------|-------------|
| `ObjCMessageExpr` | `[obj method:arg]` — the ObjC call; `getSelector()`, `getReceiverKind()` |
| `ObjCPropertyRefExpr` / `ObjCSubscriptRefExpr` / `ObjCIvarRefExpr` / `ObjCIsaExpr` | property / `obj[i]` / ivar / `->isa` access — the first two ride inside `PseudoObjectExpr` (Part 22) |
| `ObjCStringLiteral` / `ObjCBoolLiteralExpr` / `ObjCArrayLiteral` / `ObjCDictionaryLiteral` / `ObjCBoxedExpr` | `@"s"`, `@YES`, `@[…]`, `@{…}`, `@(expr)` |
| `ObjCEncodeExpr` / `ObjCSelectorExpr` / `ObjCProtocolExpr` | `@encode(T)`, `@selector(m:)`, `@protocol(P)` |
| `ObjCAvailabilityCheckExpr` | `@available(macOS 15, *)` |
| `ObjCIndirectCopyRestoreExpr` | ARC writeback for `NSError **`-style out-params — compiler-generated only |

Then the accelerator and platform extensions:

| Node | World | Produces it |
|------|-------|-------------|
| `ArraySectionExpr` | OpenMP/OpenACC | `arr[1:n]` in a `#pragma` clause |
| `OMPArrayShapingExpr` | OpenMP | `([n][m])ptr` reshaping in clauses |
| `OMPIteratorExpr` | OpenMP | `iterator(i = 0:n)` in `depend` clauses |
| `OpenACCAsteriskSizeExpr` | OpenACC | the `*` in `gang(num:*)` |
| `HLSLOutArgExpr` | HLSL (shaders) | `out`/`inout` argument copy-back |
| `SYCLUniqueStableNameExpr` | SYCL | `__builtin_sycl_unique_stable_name(T)` |

With that, cross-check complete: every one of the expression kinds in this
LLVM's `StmtNodes.inc` is covered in Parts 14–24 — the majors with sections
and verified dumps, the platform tails with the tables above, and the index
in [`node_index.md`](node_index.md) maps each name to its home.

**Quiz.** A teammate's modernizer rewrites `a != b` to use a new helper.
Their matcher `binaryOperator(hasOperatorName("!="))` passes all tests on
the C++17 codebase, then misses dozens of sites after the codebase adopts
`operator<=>`. Why — and what's the two-matcher fix?

> [!hint]- Hint
> §24.2. What node is `a != b` when `!=` was never declared but `==` was?

> [!success]- Answer
> Under `<=>`/defaulted `==`, the `!=` is a `CXXRewrittenBinaryOperator`
> (semantic form `!(a == b)`), not a `BinaryOperator` — different node
> kind, so the matcher never fires. Fix:
> `anyOf(binaryOperator(hasOperatorName("!=")), cxxRewrittenBinaryOperator(hasOperatorName("!=")))`
> — and for class types with a *declared* `!=`, a third spelling exists:
> `cxxOperatorCallExpr(hasOverloadedOperatorName("!="))`. One source token,
> three possible nodes: the recurring moral of the expression hierarchy.
