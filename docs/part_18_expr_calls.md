# Part 18 — Expressions: Calls

[← Part 17 — Operators](part_17_expr_operators.md) | [Part 19 — Construction & Init →](part_19_expr_construction_init.md)

Calls are where interprocedural anything begins, and Clang models them as a
small family: one base class carrying callee-and-arguments, plus subclasses
for the C++ syntaxes that *are* calls without looking like them. By the end
of this part, "find every call to X" stops having false negatives.

## 18.1 — CallExpr

**`CallExpr`** is the base for all calls:

```cpp
CE->getCallee();          // an EXPRESSION (usually DeclRefExpr wrapped in casts)
CE->getDirectCallee();    // FunctionDecl* if statically known, else null
CE->getNumArgs();  CE->getArg(i);  CE->arguments();
CE->getBuiltinCallee();   // Builtin::BI__builtin_expect etc., 0 if not a builtin
```

`getCallee()` vs `getDirectCallee()` matters: the callee is an arbitrary
*expression* (function pointer, member of a struct of callbacks…), and even
for `totalArea(shapes)` it's `ImplicitCastExpr <FunctionToPointerDecay>`
around the `DeclRefExpr`. `getDirectCallee()` does the unwrapping and
returns the `FunctionDecl` when there is one — reach for it first, fall
back to analyzing the callee expression when it's null.

Arguments come *converted*: what you see in `arguments()` already contains
the implicit casts, materialized temporaries, and — for arguments you
didn't write — `CXXDefaultArgExpr` placeholders (Part 19). The as-written
argument is `CE->getArg(i)->IgnoreImplicit()` away.

Builtin calls (`__builtin_expect(x, 1)`) are ordinary `CallExpr`s whose
callee is a builtin `FunctionDecl`; `getBuiltinCallee()` is the fast
discriminator, and a handful of builtins with irregular semantics get their
own nodes instead (Part 17's `AtomicExpr`, `ShuffleVectorExpr`,
`ChooseExpr` — the AST tells you which side of the line each landed on).

## 18.2 — CXXMemberCallExpr

`shape->area()` — member *syntax* calls:

```cpp
MCE->getMethodDecl();               // CXXMethodDecl being called (static target)
MCE->getImplicitObjectArgument();   // the object: shape
MCE->getRecordDecl();               // class the method belongs to
MCE->getObjectType();               // static type of the object expression
```

The callee child is the `MemberExpr` from Part 16, so everything you know
about `isArrow()` / `isImplicitAccess()` applies inside calls too — the
bare `name()` call inside another `Shape` method is a `CXXMemberCallExpr`
whose `MemberExpr` base is an implicit `CXXThisExpr`.

Virtuality reminder: `getMethodDecl()` is the *static* target.
`MCE->getMethodDecl()->isVirtual()` plus
`MCE->getImplicitObjectArgument()->getType()` is as close as the AST gets
to devirtualization reasoning; actual dispatch is a runtime fact.

## 18.3 — CXXOperatorCallExpr

An **overloaded** operator used with operator syntax. The sample's
`doubled(totalArea(shapes))` is one (verified): calling a lambda goes
through its `operator()`, so the dump shows `CXXOperatorCallExpr 'double'
'()'` whose first child resolves to the closure's `operator()` and whose
remaining children are the object and then the arguments:

```cpp
OCE->getOperator();          // OO_Call, OO_Less, OO_PlusPlus, OO_Subscript, …
OCE->isAssignmentOp();  OCE->isComparisonOp();  OCE->isInfixBinaryOp();
OCE->getOperatorLoc();       // where the operator token is
```

Argument layout is the part worth memorizing: **for a member operator, the
object is `getArg(0)`** and the written arguments start at `getArg(1)`;
for a non-member operator, arguments are just left-to-right. The range-for
machinery in Part 12 (`!=`, `++`, `*` on iterators) and every `<<` of a
stream are these nodes — which is why operator-heavy C++ makes
"`callExpr()` matches" explode compared to what the eye sees as calls.

Two related kinds complete the member-call picture from other parts:
`CXXPseudoDestructorExpr` (`p->~int()`, Part 21) and the dependent-code
`UnresolvedMemberExpr` (Part 23).

## 18.4 — UserDefinedLiteral, revisited as a call

Part 15 introduced `UserDefinedLiteral` from the literal side; from this
side it's simply a `CallExpr` subclass whose callee is `operator""_km`.
The consequence for tools cuts both ways and is worth stating precisely:

- every `callExpr()` matcher **does** match `1.5_km` — your call counts
  include literals;
- `getDirectCallee()` works, so call-graph builders get the edge for free;
- but `getArg(0)` is the cooked literal, and there is no caller-visible
  argument list in the source to attach fix-its to.

`CE->getStmtClass() == Stmt::UserDefinedLiteralClass` (or
`isa<UserDefinedLiteral>(CE)`) is the discriminator when literal-calls need
different treatment.

## 18.5 — The call tail

The remaining two call-shaped kinds, for completeness:

| Node | Source shape | Notes |
|------|--------------|-------|
| `CUDAKernelCallExpr` | `kernel<<<blocks, threads>>>(args)` | CUDA launch syntax; a `CallExpr` subclass whose `getConfig()` is the launch configuration — only under `-x cuda` |
| `BlockExpr` | `^int (int x) { return x + 1; }` | an Apple *block* literal (closure); not itself a call, but the callable the C-side ecosystem passes around — `getBlockDecl()` is Part 10's `BlockDecl` with the body and captures |

Everything callable in the language now has a face: plain calls, member
calls, operator calls, literal calls, kernel launches — and lambdas
(`LambdaExpr`, Part 21) and blocks as the callables themselves.

**Quiz.** In `double result = doubled(totalArea(shapes));` there are two
call nodes. For each: what node class, and what does `getDirectCallee()`
return? And the follow-up that catches people: why is the outer one *not* a
`CXXMemberCallExpr`, even though it certainly calls a member function?

> [!hint]- Hint
> `clang-query`: `set output dump`, then `match
> varDecl(hasName("result"))`. Then re-read the first line of §18.2 —
> which word is doing all the work?

> [!success]- Answer
> Inner: plain `CallExpr`; `getDirectCallee()` → the `FunctionDecl` for
> `totalArea` (after decay-cast unwrapping). Outer: `CXXOperatorCallExpr`;
> `getDirectCallee()` → the closure type's `CXXMethodDecl operator()`.
> `CXXMemberCallExpr` is reserved for member *syntax* — `obj.f()` /
> `ptr->f()`. `doubled(…)` uses call syntax on an object, so it's the
> operator-call node; same callee kind, different spelling, different node.
