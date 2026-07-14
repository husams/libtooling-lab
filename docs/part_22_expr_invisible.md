# Part 22 — Expressions: The Invisible AST

[← Part 21 — C++ Objects](part_21_expr_cpp_objects.md) | [Part 23 — Dependence & Packs →](part_23_expr_dependent_packs.md)

Beyond implicit casts (Part 20), Clang inserts a second stratum of nodes no
programmer ever types: lifetime bookkeeping for temporaries, value-caching
placeholders, constant-result wrappers — and, when your code doesn't even
parse, error placeholders. This part accounts for all of them, then teaches
the `Ignore*` helpers that peel the whole stratum away on demand.

## 22.1 — Temporaries: the lifetime trio

Three nodes manage object lifetime inside expressions, and they always
arrive as a team. The sample's `Circle` constructor initializes its base
with `Shape("circle")`, and the dump is the canonical specimen:

```
ExprWithCleanups 'Shape'
`-CXXConstructExpr 'Shape' 'void (std::string)'
  `-CXXBindTemporaryExpr 'std::string' (CXXTemporary …)
    `-ImplicitCastExpr <ConstructorConversion>
      `-CXXConstructExpr 'std::string' 'void (const char *)'
        `-ImplicitCastExpr <ArrayToPointerDecay>
          `-StringLiteral "circle"
```

Reading bottom-up: the literal decays to `const char*`; a `std::string`
temporary is constructed from it; **`CXXBindTemporaryExpr`** marks that this
temporary needs its destructor run at the end of the full-expression;
**`ExprWithCleanups`** is that full-expression boundary — the node that says
"after evaluating me, run the pending destructors." (Its base class,
`FullExpr`, is shared with `ConstantExpr` below — the two "evaluation
boundary" nodes.) The third team member, **`MaterializeTemporaryExpr`**,
converts a prvalue into an object with a location (an xvalue) — you saw it
materialize the hidden array behind `initializer_list` in Part 19, and it's
the node that implements lifetime-extension when a temporary binds to a
`const&`:

```cpp
MTE->getStorageDuration();      // SD_FullExpression, SD_Static, SD_Automatic…
MTE->getExtendingDecl();        // the VarDecl whose reference extends it, if any
```

For tools, the practical takeaway: **"how many `std::string` temporaries
does this function create"** is a countable AST question
(`match materializeTemporaryExpr()` / `cxxBindTemporaryExpr()` in
clang-query) — performance linters like
`performance-unnecessary-value-param` are built on exactly these nodes.

## 22.2 — Placeholders: OpaqueValueExpr and PseudoObjectExpr

**`OpaqueValueExpr`** is Clang's "evaluate once, reference twice" device: a
placeholder for a value that some enclosing node computes and reuses. The
easiest place to see one is the GNU conditional `x ?: 7` — a
**`BinaryConditionalOperator`** (Part 17) whose condition and true-branch
are *the same evaluation*, so the tree stores the shared sub-expression once
and points at it through `OpaqueValueExpr`s (verified: four of them in the
one-line probe). You also met it inside `ArrayInitLoopExpr` (Part 19), and
it reappears wherever semantics demand sharing without re-evaluation.

**`PseudoObjectExpr`** generalizes the trick for *abstract* accesses that
compile into calls: Objective-C properties, and MS `__declspec(property)`:

```cpp
struct MSObj {
  int get_x(); void put_x(int);
  __declspec(property(get = get_x, put = put_x)) int x;
};
int useprop(MSObj &o) { return o.x; }    // -fms-extensions
```

The `o.x` is a `PseudoObjectExpr` holding a *syntactic form*
(`MSPropertyRefExpr`, Part 16's table) plus the *semantic form* — the
`get_x()` call that actually runs (verified). Same syntactic/semantic split
you saw in `InitListExpr`, applied to member access.

## 22.3 — ConstantExpr and RecoveryExpr: the boundary markers

**`ConstantExpr`** wraps expressions the language *requires* to be constant —
array bounds, `case` values (verified in the switch probe), template
arguments — and caches the evaluated result right in the node
(`getAPValueResult()`). When you see it, constant evaluation already
happened and succeeded.

**`RecoveryExpr`** is its dark twin: the parse-or-semantic-error
placeholder. `int broken = undeclared_call(42);` still produces an AST —

```
VarDecl broken 'int' cinit
`-RecoveryExpr '<dependent type>' contains-errors lvalue
  `-IntegerLiteral 42
```

(verified). The salvaged pieces are kept as children; the type is dependent;
`containsErrors()` is set on everything above it. Two practical
consequences. First, this is what Part 1's quiz warned about: tools running
on broken code silently traverse these. Second, well-behaved checks bail
early — `if (E->containsErrors()) return;` — because every "impossible"
crash report against clang-tidy starts with an AST full of `RecoveryExpr`.

## 22.4 — The Ignore* helpers

Almost every check wants "the expression the programmer wrote," so `Expr`
ships a family of strippers:

```cpp
E->IgnoreParens();           // (((x)))            → x
E->IgnoreImpCasts();         // implicit casts only
E->IgnoreParenImpCasts();    // both — the workhorse
E->IgnoreParenCasts();       // parens + ALL casts, even explicit ones
E->IgnoreImplicit();         // impl. casts + FullExpr + MaterializeTemporary + bind
E->IgnoreUnlessSpelledInSource();  // maximal: everything not typed by the user
```

Choosing matters. `IgnoreParenImpCasts()` is right for "is the operand a
literal?" (`total += 1` — peel `LValueToRValue`). It is *wrong* for a check
that must distinguish `x` from `(T)x` — there `IgnoreParenCasts()` would
erase the evidence. And `IgnoreUnlessSpelledInSource()` is the expression-
level twin of the matcher traversal mode from Part 33: aggressive,
convenient, and occasionally surprising (it looks through
`CXXDefaultArgExpr` boundaries too). Rule of thumb: strip the minimum that
makes your question well-posed.

**Quiz.** Your check wants to flag `if (assignment)` typos — conditions
that are assignments rather than comparisons. Starting from
`IfStmt::getCond()`, which stripper do you apply before `dyn_cast<BinaryOperator>`,
and what breaks if you use `IgnoreParenCasts()` instead?

> [!hint]- Hint
> The condition of an `if` undergoes contextual conversion to `bool` — that
> alone tells you which invisible nodes sit on top. And one idiom uses
> extra parens *deliberately*: `if ((x = next()))`.

> [!success]- Answer
> `IgnoreParenImpCasts()` — the condition arrives wrapped in implicit
> conversions (to `bool`, plus `LValueToRValue`), and possibly parens.
> With `IgnoreParenCasts()` you'd also strip an *explicit*
> `(bool)(x = y)` cast, flagging code whose author already announced the
> intent — and you lose the ability to honor the double-parens idiom,
> which requires seeing the `ParenExpr` layer before deciding.

## 22.5 — Reading a full sandwich

Put the strata together and a one-line source statement routinely dumps as
seven layers:

```
ExprWithCleanups                    ← full-expression boundary   (22.1)
`-CXXConstructExpr                  ← the object being made      (19.1)
  `-CXXBindTemporaryExpr            ← destructor bookkeeping     (22.1)
    `-ImplicitCastExpr              ← a conversion               (20.2)
      `-CXXConstructExpr            ← the temporary itself       (19.1)
        `-ImplicitCastExpr          ← another conversion         (20.2)
          `-StringLiteral           ← what you actually wrote    (15)
```

The test of these five parts is that this tower now reads as routine: each
line has a part number, each node an owner. When a dump still contains a
mystery node after Part 24, the workflow from Part 2 §2.5 — dump, query,
header — closes the gap.
