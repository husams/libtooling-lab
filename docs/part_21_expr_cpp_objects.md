# Part 21 — Expressions: C++ Objects & Introspection

[← Part 20 — Casts](part_20_expr_casts.md) | [Part 22 — The Invisible AST →](part_22_expr_invisible.md)

This part collects the expressions that make C++ object-oriented and
self-describing: `this`, dynamic allocation, exceptions, `typeid`, lambdas,
and the compiler's own trait intrinsics. Several of these are the *only*
place a whole language feature surfaces in the AST — miss the node, miss the
feature.

## 21.1 — CXXThisExpr

```cpp
TE->isImplicit();      // spelled `this`, or inserted by the compiler?
```

`CXXThisExpr` shows up far more often than the keyword `this` does: inside
`Circle::area()`, the bare `radius_` is really `this->radius_`, and the AST
spells it out — a `MemberExpr` (marked implicit) over a `CXXThisExpr`
(marked implicit). Field-access checks must handle this or they'll only
match explicitly-qualified accesses. Its type is `Circle *` (or
`const Circle *` in const methods) — asking `getType()` here is the standard
way a check discovers "which class am I inside" from expression context.

## 21.2 — CXXNewExpr and CXXDeleteExpr

```cpp
CXXNewExpr:    NE->getAllocatedType();  NE->isArray();  NE->getArraySize();
               NE->getConstructExpr();  NE->getOperatorNew();
               NE->getNumPlacementArgs(); NE->placement_arguments();
CXXDeleteExpr: DE->getArgument();  DE->isArrayForm();   // delete vs delete[]
               DE->getDestroyedType();  DE->getOperatorDelete();
```

`CXXNewExpr` bundles four concerns into one node: the allocation function
(`operator new`, possibly placement), the array-ness, the constructor call
for the object(s), and the resulting pointer type. The `delete`/`delete[]`
mismatch bug class lives entirely in `isArrayForm()` checked against how the
pointee was allocated — and "flag every raw `new`" (the
`cppcoreguidelines-owning-memory` genre) is a one-line `cxxNewExpr()`
matcher plus taste.

## 21.3 — Exceptions: CXXThrowExpr and CXXNoexceptExpr

**`CXXThrowExpr`** — `throw e` or the rethrow `throw;`
(`getSubExpr()` is null for the latter). It's an *expression* of type
`void`, which is why `cond ? throw x : value` parses.

**`CXXNoexceptExpr`** — the *query* `noexcept(expr)`: compile-time, boolean,
never evaluates its operand (verified: `noexcept(1 + 1)` in the probe).
`getValue()` hands you the answer Clang computed. Don't confuse it with the
`noexcept` *specifier* on function types — that lives in
`FunctionProtoType` (Part 27), not in any expression.

## 21.4 — Runtime introspection: typeid and friends

**`CXXTypeidExpr`** — `typeid(e)` / `typeid(T)`; result type
`const std::type_info&` (verified in the probe, with just a forward
declaration of `std::type_info` in scope):

```cpp
TIE->isTypeOperand();          // typeid(T) vs typeid(expr)
TIE->getTypeOperand(Ctx);      // the T
TIE->isPotentiallyEvaluated(); // typeid(*p) on a polymorphic type — the
                               // one case that can THROW (null deref)
```

**`CXXPseudoDestructorExpr`** — the curiosity `p->~I()` where `I` is a
scalar's typedef: syntactically a destructor call, semantically a no-op that
exists so templates can uniformly write `t.~T()` (verified). Generic-code
analyzers meet it; everyone else learns it exists right here.

`CXXUuidofExpr` (`__uuidof`, MS COM) rounds out the family — table
knowledge only.

## 21.5 — LambdaExpr

A lambda is a compressed class definition, and **`LambdaExpr`** gives you
both views:

```cpp
LE->captures();              // each capture: kind (by-ref/by-copy/init), the VarDecl
LE->capture_inits();         // the initialization exprs for the captured fields
LE->getCallOperator();       // the CXXMethodDecl operator()
LE->getLambdaClass();        // the closure CXXRecordDecl (Part 6's implicit class)
LE->getBody();               // the CompoundStmt
LE->hasExplicitParameters(); LE->getIntroducerRange();
```

The sample's `[](double v) { return 2 * v; }` produces a `LambdaExpr` whose
closure class contains the `operator()` you saw being called via
`CXXOperatorCallExpr` in Part 18 — plus, for this captureless lambda, a
declared conversion function to `double (*)(double)` and its `__invoke`
thunk. Captures with initializers (`[n = compute()]`) declare real
`FieldDecl`s in the closure class; a visitor that recurses into lambda
bodies is walking a nested class definition whether it knows it or not
(which is why Part 30's `TraverseCXXRecordDecl`-skipping trick calls
`isLambda()`).

**Quiz.** A colleague's visitor counts "functions defined in the main file"
via `VisitFunctionDecl` + `isThisDeclarationADefinition()`, and the single
lambda line contributes **two** definitions. Which two — and why doesn't
the conversion function `operator double (*)(double)` count?

> [!hint]- Hint
> `clang-query`:
> `match functionDecl(isDefinition(), isExpansionInMainFile(), hasAncestor(cxxRecordDecl(isLambda())))`
> — then check which members are merely *declared*.

> [!success]- Answer
> The call operator `operator()` (the only user code) and the closure's
> implicit *destructor* — `main` destroys `doubled`, so its defaulted body
> gets defined. The conversion function and `__invoke` are declared but
> never used, so their definitions are never instantiated. Filtering with
> `isImplicit()` keeps just the call operator — the standard fix for
> inflated per-function metrics on lambda-heavy code.

## 21.6 — Compiler trait intrinsics

The `<type_traits>` library bottoms out in compiler builtins, and each
builtin family has a node:

- **`TypeTraitExpr`** — the big one: `__is_trivially_copyable(Agg)`,
  `__is_same(T, U)`, `__is_constructible(T, Args…)` — N-ary, over *types*
  (verified). `getTrait()` says which; `getValue()` the verdict.
- **`ArrayTypeTraitExpr`** — `__array_rank(int[2][3])`,
  `__array_extent(T, I)` (verified).
- **`ExpressionTraitExpr`** — `__is_lvalue_expr(e)` / `__is_rvalue_expr(e)`
  (verified) — over *expressions*, ancient GCC compatibility.

You'll rarely match these directly, but every time you step through how
`std::is_same_v` became `true` in a dump, this is the floor you hit:
`static_assert(std::is_same_v<…>)` in libc++ is sugar over a
`TypeTraitExpr` a few template layers down.
