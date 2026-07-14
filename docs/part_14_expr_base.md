# Part 14 — Expressions: The Expr Base

[← Part 13 — Statements: The Long Tail](part_13_stmts_long_tail.md) | [Part 15 — Literals →](part_15_expr_literals.md)

Expressions are the largest node family in the AST — eleven parts' worth —
and every one of them shares the `Expr` base. Before touring the kinds, this
short part nails down the three properties stamped on *every* expression
node: its type, its value category, and its dependence. These three explain
most of what a dump shows beyond the node names.

## 14.1 — Expr in the hierarchy

`Expr` derives from `ValueStmt` derives from `Stmt` — an expression *is* a
statement, which is why `doubled(totalArea(shapes));` can sit directly in a
`CompoundStmt` with no wrapper. What `Expr` adds to the statement interface
is the semantic payload:

```cpp
E->getType();                   // QualType — the expression's static type
E->getValueKind();              // VK_LValue / VK_XValue / VK_PRValue
E->getObjectKind();             // OK_Ordinary, OK_BitField, OK_VectorComponent…
E->isInstantiationDependent();  // does this involve template parameters?
```

`getType()` alone answers half of all tooling questions — but remember it's
the *static* type. The AST cannot know that `shape->area()` dispatches to
`Circle::area` at runtime; it records the call's static target and type,
period.

## 14.2 — Value categories

Value categories are not academic here — they're stored on every node, and
the dump prints them (`lvalue`, `xvalue`; prvalue is the unmarked default):

```cpp
E->isLValue();     // names a persistent object:  total,  shape->radius_
E->isXValue();     // expiring value:  std::move(s),  materialized temporaries
E->isPRValue();    // pure value:  42,  x + y,  totalArea(shapes)
E->isGLValue();    // lvalue or xvalue — "has identity"
```

That single annotation explains most of the `ImplicitCastExpr
<LValueToRValue>` nodes you've been seeing since Part 2: *using* an lvalue
where a value is needed is itself a conversion, and Clang writes it down.
It also explains xvalues' whole reason to exist — `std::move(s)` is nothing
but a cast to `std::string&&` whose result category is xvalue; no code, only
category.

When you need the finer classification the standard uses (bit-fields,
vector components, function designators), there's
`E->Classify(Ctx)` returning an `Expr::Classification` — rarely needed, but
it's the authoritative answer.

## 14.3 — Object kinds

`getObjectKind()` is the companion enum almost nobody notices until it
bites: an lvalue that refers to a **bit-field** (`OK_BitField`) or a vector
component (`OK_VectorComponent`) can't have its address taken, and tools
generating "take the address of this expression" fix-its must check it.
Ordinary expressions are `OK_Ordinary`, and the remaining kinds
(`OK_MatrixComponent`, the ObjC property/subscript kinds) mark the other
"lvalue-ish things with no address" cases.

## 14.4 — ParenExpr

The parentheses you write are nodes too: `(total)` is a `ParenExpr` wrapping
the `DeclRefExpr`. Semantically inert, structurally present — and therefore
the first member of the *wrapper* club that makes naive structural matching
fail:

```cpp
PE->getSubExpr();      // what's inside
E->IgnoreParens();     // strip any number of ParenExpr (and a few paren-like nodes)
```

`IgnoreParens()` is the entry point to the `Ignore*` helper family. The full
set — and the discipline of choosing the right one — is Part 22's subject,
after you've met the implicit nodes they skip. Until then, one rule of
thumb: any check comparing an expression's *shape* ("is the operand a
call?") should strip parens first, because `(f())` and `f()` differ by one
node and by nothing.

## 14.5 — Dependence

Inside a template, expressions may not yet have a type or value —
`value < lo` in the sample's `clamp` pattern can't be resolved until `T` is
known. The base class records this as *dependence bits*:

```cpp
E->isTypeDependent();          // type unknown until instantiation
E->isValueDependent();         // value unknown (e.g. sizeof(T))
E->isInstantiationDependent(); // any dependence at all — the broad test
E->containsUnexpandedParameterPack();   // …T args, before expansion
```

Dumps of dependent code show types like `'<dependent type>'`, and whole
node kinds exist only in dependent code (`UnresolvedLookupExpr`,
`CXXDependentScopeMemberExpr`, …) — Part 23 tours them. The base-class
rule to internalize now: **check dependence before asking semantic
questions.** `getType()` on a type-dependent expression, or Part 24's
`EvaluateAsInt` on a value-dependent one, cannot give you the answer the
instantiated code will have.

**Quiz.** In `total += shape->area()`, classify each of these by value
category: `total`, `shape`, `shape->area()`, and the whole `+=` expression.
One of the four is different in C than in C++ — which?

> [!hint]- Hint
> Ask "does it have identity — can I take its address?" for each. For the
> last one, recall that C++ assignment yields the assigned object;
> C's yields only its value.

> [!success]- Answer
> `total` — lvalue. `shape` — lvalue (it's a named variable; the *use* of
> it gets an `LValueToRValue` cast). `shape->area()` — prvalue (returns
> `double` by value). The whole `+=` — lvalue in C++ (it denotes `total`;
> you can write `(total += x) = 0`), but a *non-lvalue* in C. That
> difference is stored, not computed: the same source dumps with different
> value-kind annotations under `-x c` vs `-x c++`.
