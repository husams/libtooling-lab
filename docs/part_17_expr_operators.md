# Part 17 — Expressions: Operators

[← Part 16 — Names & Members](part_16_expr_names_members.md) | [Part 18 — Calls →](part_18_expr_calls.md)

Built-in operators funnel into a handful of node classes discriminated by
opcode — a design that keeps the AST small and your `switch` statements
long. This part covers the operator nodes proper, the C++20 rewritten
comparisons, subscripts, the compile-time size/offset operators, and the
complete GNU/builtin operator tail.

## 17.1 — UnaryOperator

**`UnaryOperator`** is one class with fourteen opcodes — the complete set:

| Opcode | Source | | Opcode | Source |
|--------|--------|-|--------|--------|
| `UO_PostInc` | `i++` | | `UO_Minus` | `-x` |
| `UO_PostDec` | `i--` | | `UO_Not` | `~x` |
| `UO_PreInc` | `++i` | | `UO_LNot` | `!x` |
| `UO_PreDec` | `--i` | | `UO_Real` | `__real__ c` (GNU) |
| `UO_AddrOf` | `&x` | | `UO_Imag` | `__imag__ c` (GNU) |
| `UO_Deref` | `*p` | | `UO_Extension` | `__extension__ e` |
| `UO_Plus` | `+x` | | `UO_Coawait` | `co_await` (as parsed; see Part 24) |

```cpp
UO->getOpcode();   UO->getSubExpr();
UO->isPrefix();    UO->isPostfix();     // inc/dec forms are DIFFERENT opcodes, not a flag
UO->isIncrementDecrementOp();
```

Pre/post increment being separate opcodes (not a flag) is deliberate: they
differ in value category (`++i` is an lvalue in C++, `i++` a prvalue) and
that's stored per node, as Part 14 promised.

## 17.2 — BinaryOperator and CompoundAssignOperator

**`BinaryOperator`** covers every built-in binary op — all 33 opcodes:

| Group | Opcodes |
|-------|---------|
| pointer-to-member | `BO_PtrMemD` (`.*`), `BO_PtrMemI` (`->*`) |
| multiplicative | `BO_Mul`, `BO_Div`, `BO_Rem` |
| additive | `BO_Add`, `BO_Sub` |
| shifts | `BO_Shl`, `BO_Shr` |
| three-way | `BO_Cmp` (`<=>` on built-in types) |
| relational | `BO_LT`, `BO_GT`, `BO_LE`, `BO_GE` |
| equality | `BO_EQ`, `BO_NE` |
| bitwise | `BO_And`, `BO_Xor`, `BO_Or` |
| logical | `BO_LAnd`, `BO_LOr` |
| assignment | `BO_Assign` |
| compound assign | `BO_MulAssign`, `BO_DivAssign`, `BO_RemAssign`, `BO_AddAssign`, `BO_SubAssign`, `BO_ShlAssign`, `BO_ShrAssign`, `BO_AndAssign`, `BO_XorAssign`, `BO_OrAssign` |
| sequencing | `BO_Comma` |

```cpp
BO->getOpcode();      BO->getOpcodeStr();       // "+", "<", "=", …
BO->getLHS();         BO->getRHS();
BO->isAssignmentOp(); BO->isComparisonOp(); BO->isLogicalOp(); BO->isBitwiseOp();
```

The ten compound-assign opcodes always come as the subclass
**`CompoundAssignOperator`**, which adds the computation types —
`totalArea`'s `total += shape->area()` dumps as `CompoundAssignOperator …
'+=' ComputeLHSTy='double'`, recording the type the implicit `lhs + rhs`
is computed in (which can differ from both operands in mixed-width
arithmetic — the classic `char c; c += 1000;` question).

One boundary to keep sharp: these nodes are **built-in operators only**.
`s1 + s2` on `std::string` is *not* a `BinaryOperator` — it's a
`CXXOperatorCallExpr` (Part 18). A check matching only
`binaryOperator(hasOperatorName("=="))` silently ignores every overloaded
`==` in the codebase.

## 17.3 — Conditionals

**`ConditionalOperator`** is `c ? a : b` — `getCond()`, `getTrueExpr()`,
`getFalseExpr()`. Its GNU sibling **`BinaryConditionalOperator`** is
`x ?: y` ("use `x` unless it's zero/null"), where the condition and the
true-arm are *the same evaluation* — the node stores it once as
`getCommon()` and threads it through an `OpaqueValueExpr` (Part 22's
placeholder machinery, making its first natural appearance). Both share
the abstract base `AbstractConditionalOperator`, which is what generic
code should match against.

## 17.4 — CXXRewrittenBinaryOperator

C++20's `<=>`-driven comparison rewriting gets an explicit node,
**`CXXRewrittenBinaryOperator`**. For the sample probe `struct P { auto operator<=>(const P&) const = default; };`,
the expression `a < b` dumps as (verified):

```
CXXRewrittenBinaryOperator 'bool'
└─CXXOperatorCallExpr 'bool' '<' adl
  ├─… operator<=>(a, b) …
  └─IntegerLiteral 0
```

The node wraps the *rewritten form* — `(a <=> b) < 0` — while remembering
what you wrote:

```cpp
RBO->getOpcode();          // BO_LT: the operator as WRITTEN
RBO->isReversed();         // true when rewritten as (b <=> a) with swapped args
RBO->getSemanticForm();    // the actual call tree that executes
RBO->getLHS(); RBO->getRHS();   // convenience views into the semantic form
```

This is the syntactic-vs-semantic split you'll meet again in Part 19's
`InitListExpr`: one node, two shapes, and every analysis must decide which
shape it cares about. Style checks read the written form; codegen-ish
reasoning reads the semantic form.

## 17.5 — Subscripts

**`ArraySubscriptExpr`** — `arr[i]`: `getBase()`, `getIdx()` — and remember
C's `2[arr]` is legal, so "base" is *computed* (the pointer-typed operand),
not positional; `getLHS()`/`getRHS()` give the as-written order. The
matrix extension adds **`MatrixSubscriptExpr`** (`m[r][c]` on
`-fenable-matrix` types, both indices in one node once complete) and, in
this LLVM, **`MatrixSingleSubscriptExpr`** for the intermediate
one-index form. Vector swizzles were Part 16's `ExtVectorElementExpr`;
OpenMP's `arr[1:n]` slice is Part 24's `ArraySectionExpr`.

## 17.6 — sizeof, alignof, offsetof

`sizeof x` and `alignof(T)` share one node, **`UnaryExprOrTypeTraitExpr`**
(the name is honest: unary expression *or* type trait):

```cpp
UETT->getKind();               // UETT_SizeOf, UETT_AlignOf, UETT_PreferredAlignOf,
                               // UETT_VecStep, UETT_OpenMPRequiredSimdAlign,
                               // UETT_DataSizeOf, UETT_PtrAuthTypeDiscriminator, …
UETT->isArgumentType();        // sizeof(double) — a type operand
UETT->getArgumentType();       //   → QualType
UETT->getArgumentExpr();       // sizeof x — an expression operand (UNEVALUATED)
```

The operand of `sizeof` is an *unevaluated context* — `sizeof(f())` calls
nothing — and the AST inside it reflects that: no temporaries, no cleanups.

**`OffsetOfExpr`** (`offsetof(S, m)`, verified dump: `OffsetOfExpr
'__size_t'`) carries a *component path* — `getComponent(i)` yields field,
array-index, or base-class steps, so `offsetof(S, a.b[2].c)` is one node
with four components. Both nodes are compile-time constants, which
previews Part 24: `E->EvaluateAsInt(…, Ctx)` folds them to the number the
compiler would use.

## 17.7 — The GNU and builtin operator tail

The complete remaining set — each a real expression kind you can now
identify on sight:

| Node | Source shape | Notes |
|------|--------------|-------|
| `AddrLabelExpr` | `&&label` | GNU; feeds Part 12's `IndirectGotoStmt` (verified pair) |
| `StmtExpr` | `({ int t = f(); t + 1; })` | GNU statement-expression; value is the last statement's — the reason `ValueStmt` exists (verified) |
| `ChooseExpr` | `__builtin_choose_expr(c, a, b)` | compile-time select, C only; `getChosenSubExpr()` |
| `GenericSelectionExpr` | `_Generic(x, int: f, double: g)` | C11 type-switch; `getResultExpr()`; detailed in Part 24 |
| `VAArgExpr` | `va_arg(ap, int)` | varargs read; verified — note its location sits inside the `__stdarg_va_arg.h` macro |
| `AtomicExpr` | `__atomic_load_n(p, __ATOMIC_SEQ_CST)`, `__c11_atomic_*` | one node for the whole builtin family; `getOp()` discriminates, verified |
| `ShuffleVectorExpr` | `__builtin_shufflevector(v1, v2, 0, 2, …)` | vector permute |
| `ConvertVectorExpr` | `__builtin_convertvector(v, int4)` | element-wise conversion |
| `AsTypeExpr` | `__builtin_astype(v, T)` | OpenCL bit-reinterpret |

**Quiz.** After `#define MAX(a,b) ((a) > (b) ? (a) : (b))` and the line
`int m = MAX(x++, y);`, you want to warn about the double evaluation. Which
two node kinds from this part does the expansion contain that the innocent-
looking source doesn't, and why does the *GNU* conditional from §17.3 not
have this bug for `x ?: y`?

> [!hint]- Hint
> Dump it: parens are nodes, and `x++` appears twice in the tree. For the
> second half — what did `getCommon()` and `OpaqueValueExpr` exist to
> guarantee?

> [!success]- Answer
> The expansion contributes `ParenExpr`s (five pairs of them) and a
> `ConditionalOperator` whose *true-arm and condition both contain a
> distinct copy* of the `UnaryOperator UO_PostInc` — two `x++` subtrees,
> hence double evaluation on the taken path. `BinaryConditionalOperator`
> stores the shared operand once (`getCommon()`), referenced twice via one
> `OpaqueValueExpr` — evaluated once by construction, which is the entire
> reason `?:`-without-middle exists.
