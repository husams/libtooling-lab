# Part 12 — Statements: Loops & Jumps

[← Part 11 — Statements: Core & Selection](part_11_stmts_core.md) | [Part 13 — Statements: The Long Tail →](part_13_stmts_long_tail.md)

Iteration and transfer of control account for nine statement kinds. Three
of the loops are exactly what they look like; the fourth —
`CXXForRangeStmt` — is the AST's best demonstration that Clang stores the
*meaning* of your code, not its spelling. The jump statements close the
part, including the two GNU extensions most tools never expect to meet.

## 12.1 — WhileStmt and DoStmt

**`WhileStmt`** is minimal: `getCond()`, `getBody()`, plus C++'s
declaration-in-condition form (`while (auto *p = next())`) via
`getConditionVariable()` / `getConditionVariableDeclStmt()`.

**`DoStmt`** is the same pair with the order reversed in source —
`getBody()`, then `getCond()` — and no condition-variable form (the
standard doesn't allow one). Its one location extra, `getWhileLoc()`, marks
the `while` keyword, which rewriting tools need because the condition sits
*after* the body's closing brace.

The famous `do { … } while (0)` macro idiom means `DoStmt` appears in
expanded code far more often than in hand-written code — check
`getBeginLoc().isMacroID()` before flagging one (Part 31 covers the
machinery).

## 12.2 — ForStmt

**`ForStmt`** is the classic three-slot header plus body — with the trap
that **all three header slots are optional**:

```cpp
For->getInit();   // Stmt*  — null in  for (; i < n; ++i)
For->getCond();   // Expr*  — null in  for (;;)
For->getInc();    // Expr*  — null in  for (i = 0; i < n; )
For->getBody();   // never null
For->getConditionVariable();   // for (; auto *p = poll(); )  — rare but legal
```

`for (;;)` gives you three nulls. Null-check every slot, every time; this
is among the most common crash sources in first tools. Note also that the
init slot is a `Stmt`, not an `Expr` — `for (int i = 0; …)` puts a whole
`DeclStmt` there, so the init can declare multiple variables.

## 12.3 — CXXForRangeStmt

The source says almost nothing — `for (const auto *shape : shapes)` — but
the AST contains the *entire desugaring*. Dump `totalArea` in the sample
and look (verified):

```
CXXForRangeStmt
├─ DeclStmt → VarDecl implicit __range1 'const ShapeList &'
├─ DeclStmt → VarDecl implicit __begin1   ← __range1.begin()
├─ DeclStmt → VarDecl implicit __end1     ← __range1.end()
├─ CXXOperatorCallExpr 'bool' '!='        ← the loop condition
├─ CXXOperatorCallExpr '++'               ← the increment
├─ DeclStmt → VarDecl shape 'const geo::Shape *'   ← YOUR loop variable, from *__begin1
└─ CompoundStmt                           ← the body
```

The `__range1`/`__begin1`/`__end1` variables are real `VarDecl`s marked
`implicit`, and the `!=`/`++`/`*` calls are real, fully resolved
`CXXOperatorCallExpr`s into `std::vector`'s iterator. Role accessors:

```cpp
FR->getRangeInit();      // the expression after the colon: shapes
FR->getLoopVariable();   // your VarDecl: shape
FR->getBody();
FR->getBeginStmt(); FR->getEndStmt(); FR->getCond(); FR->getInc();  // the machinery
FR->getInit();           // C++20: for (init; var : range)
```

A "find all loop conditions" tool that only handles `ForStmt` silently
misses every modern loop; a naive statement-counter counts one range-for as
eight statements. Both bugs come from the same fact: **sugar in the source,
full expansion in the tree.**

**Quiz.** `clang-query`, sample file: `match varDecl(hasName("__begin1"))`
finds a variable no one declared. Will
`match varDecl(isExpansionInMainFile())` include it too — and what single
call on the `VarDecl` distinguishes it from user code?

> [!hint]- Hint
> The dump line reads `VarDecl … col:26 implicit used __begin1 …` — one of
> those words is also a method.

> [!success]- Answer
> Yes — its location *is* in the main file (column 26 of the for-line;
> locations point where the desugaring anchors). The discriminator is
> `VD->isImplicit()`, true for all compiler-synthesized declarations. Tools
> that report on variables should skip implicit ones or they'll warn about
> `__range1`'s naming style.

## 12.4 — ReturnStmt

**`ReturnStmt`** ends a function with an optional value:

```cpp
RS->getRetValue();       // Expr* — null for a bare `return;`
RS->getNRVOCandidate();  // VarDecl* — non-null when this return is an NRVO site
```

Two habits worth forming. First, the returned expression is usually wrapped
in an `ImplicitCastExpr` (`totalArea`'s `return total;` carries
`<LValueToRValue>`) — reach through it with `IgnoreImpCasts()` when you
want the "real" value. Second, `getNRVOCandidate()` is the AST's record of
the named-return-value optimization: when non-null, the returned local is
constructed directly in the return slot and **no copy/move constructor call
exists in the tree** — a fact that surprises tools counting copies.

In a coroutine, `return` is ill-formed; the analogous node is
`CoreturnStmt` (Part 13).

## 12.5 — BreakStmt and ContinueStmt

**`BreakStmt`** and **`ContinueStmt`** are childless leaves — and,
importantly, **they carry no pointer to the construct they exit**. Recovering "which loop does this `break` leave?"
is a parent-walk job (`ASTContext::getParents`, Part 30): climb until you
hit a loop or `SwitchStmt` node. That also encodes the language rule
tools sometimes forget — `break` binds to the nearest enclosing loop *or
switch*, `continue` only to loops.

## 12.6 — GotoStmt, LabelStmt, IndirectGotoStmt

Ordinary `goto` is two cooperating nodes plus a declaration. Verified
shape:

```
GotoStmt 'done' 0x93e…10c8            ← points at the LabelDecl
LabelStmt 'done'                       ← the target, wrapping its statement
```

```cpp
GS->getLabel();          // LabelDecl* — the edge is resolved for you
LS->getDecl();           // the same LabelDecl, from the target side
```

The GNU computed-goto extension appears more than you'd hope in
interpreters and generated parsers, and it splits into two nodes: taking a
label's address — `void *tgt = &&done;` — is an `AddrLabelExpr` (an
*expression*, detailed in Part 17), and jumping through it — `goto *tgt;` —
is an **`IndirectGotoStmt`** (verified dump: `AddrLabelExpr 'void *' done`
feeding an `IndirectGotoStmt`). Unlike `GotoStmt`, an indirect goto's
possible targets are *not* resolved edges; `getTarget()` is just the
expression, and any label whose address was taken is fair game — which is
exactly why CFG-building tools treat it as an edge to every
address-taken label in the function.
