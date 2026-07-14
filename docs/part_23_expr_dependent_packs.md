# Part 23 — Expressions: Dependence, Packs & Concepts

[← Part 22 — The Invisible AST](part_22_expr_invisible.md) | [Part 24 — Constant Eval & Misc →](part_24_expr_consteval_misc.md)

Inside a template *pattern*, semantic analysis runs out of information:
types are unknown, names can't be resolved, overloads can't be chosen. The
AST doesn't fake answers — it stores dedicated "not yet" nodes, and this
part covers all of them, plus the parameter-pack machinery and the C++20
concepts expressions. If your tool will ever walk template code (it will),
this is the part that keeps it honest.

## 23.1 — Dependent trees: what changes

Dump the sample's `clamp` pattern (`-ast-dump-filter geo::clamp`) and
compare the two trees under the one `FunctionTemplateDecl` (Part 7's
two-level model):

```
FunctionDecl clamp 'T (T, T, T)'                     ← the pattern
  … BinaryOperator '<dependent type>' '<'            ← type unknown until T is known
      DeclRefExpr 'T' … 'value'                      ← no LValueToRValue casts!
FunctionDecl clamp 'int (int, int, int)' implicit_instantiation
  … BinaryOperator 'bool' '<'
      ImplicitCastExpr 'int' <LValueToRValue> …      ← now fully checked
```

The pattern's operator has **`<dependent type>`**, and note what's missing:
no implicit casts anywhere — semantic analysis stopped at "can't know yet."
The instantiation is a complete, ordinary tree. This is the deep reason for
Part 30's `shouldVisitTemplateInstantiations()` switch: pattern and
instantiations are *different trees* answering different questions.

The cheap tests, available on every `Expr`:

```cpp
E->isTypeDependent();            // its type mentions a template parameter
E->isValueDependent();           // its value does (array bound uses N…)
E->isInstantiationDependent();   // anything at all depends — the broad test
```

## 23.2 — Unresolved names and members

When dependence hits name lookup itself, five nodes stand in for answers:

| Node | Example inside `template<class T> void f(T t)` |
|------|-----------------------------------------------|
| `DependentScopeDeclRefExpr` | `T::static_member()` — scope depends on T |
| `CXXDependentScopeMemberExpr` | `t.method()` — member of a dependent object |
| `UnresolvedLookupExpr` | `g(t)` — candidates found, final choice awaits ADL at instantiation |
| `UnresolvedMemberExpr` | member overload set not yet chosen |
| `CXXUnresolvedConstructExpr` | `T(t)` — constructing a dependent type |

All of them carry `'<dependent type>'` and none carry a resolved
declaration — `getDecl()`-style APIs don't exist on them. The two
`Unresolved*` nodes derive from `OverloadExpr`, which at least hands you the
candidate set (`decls_begin()`/`decls_end()`); the `Dependent*` nodes can't
even do that. Tools that walk template patterns must budget for "the answer
is *unknowable here*", which is why many clang-tidy checks simply skip
dependent code (`isInstantiationDependent()` is the cheap test).

## 23.3 — Parameter packs

Every pack operation has its own node — all verified in this lab's probes:

- **`PackExpansionExpr`** — the `ts...` in `sink(ts...)`: wraps the pattern
  to repeat; `getPattern()`, and `getNumExpansions()` once known.
- **`SizeOfPackExpr`** — `sizeof...(ts)`: yields the arity, no expansion.
  In an *instantiation* the dump shows the resolved number.
- **`CXXFoldExpr`** — `(ts + ...)` / `(ts + ... + 0)`: unary vs binary fold,
  `getOperator()`, `getInit()` for the seed, left vs right via
  `isLeftFold()`. In the pattern it's one node; after instantiation the
  dump shows the fully unrolled operator chain.
- **`PackIndexingExpr`** — C++26's `ts...[0]` (compiles today under
  `-std=c++2c`, verified). Type-level sibling `PackIndexingType` is
  Part 28's.
- **`FunctionParmPackExpr`** and **`SubstNonTypeTemplateParmPackExpr`** —
  transient markers for a *partially* substituted pack (e.g. while a
  generic lambda's enclosing template is instantiated but its own pack
  isn't). You'll essentially never match them; a fully-instantiated dump
  has already replaced them. Their singular sibling
  **`SubstNonTypeTemplateParmExpr`** *is* common: in `getN<3>()`'s
  instantiation, the `return N;` becomes one wrapping `IntegerLiteral 3` —
  the breadcrumb that a literal came from a template argument (verified).

That breadcrumb matters practically: a "magic number" linter that ignores
`SubstNonTypeTemplateParmExpr` wrappers will flag every use of a non-type
template parameter as a hardcoded constant.

## 23.4 — Concepts expressions

C++20 constraint machinery surfaces as two expression nodes (declarations
side: Part 8):

**`ConceptSpecializationExpr`** — a concept applied to arguments:
`Addable<int>`, whether in a `static_assert`, a `requires`-clause, or an
`if constexpr`. It's a *prvalue bool*, and the node caches satisfaction:
`isSatisfied()` (verified: two in the probe — one in the constraint, one in
the `static_assert`).

**`RequiresExpr`** — the `requires(T t) { t + t; }` body itself: a list of
requirements (simple, type, compound `{ e } noexcept -> C<>`, nested), each
its own `concepts::Requirement` object rather than an `Expr` (verified).
`getRequirements()`, `isSatisfied()`, and — for diagnostics tooling —
`getLocalParameters()` for that phantom `T t` that is never really
constructed.

**Quiz.** In the pattern of `template<Addable T> T twice(T t) { return
t + t; }`, is `t + t` an `UnresolvedLookupExpr`, a plain `BinaryOperator`,
or a `CXXOperatorCallExpr` — and does the answer change once `T = int` is
instantiated?

> [!hint]- Hint
> Overload resolution for `+` on a dependent type can't run — but the node
> for "some `+` happens here" doesn't need lookup to exist. Compare both
> trees under the `FunctionTemplateDecl` in a dump.

> [!success]- Answer
> In the pattern it's a `BinaryOperator` with type `'<dependent type>'` —
> Clang keeps the operator *syntax* and defers the meaning (no
> `UnresolvedLookupExpr`; that node is for *name* lookup, and `+` isn't a
> name here). After instantiation with `int` it stays a `BinaryOperator`,
> now typed `int`. Had `T` been a class with `operator+`, the instantiation
> would instead contain a `CXXOperatorCallExpr` — same source, three
> different nodes depending on what `T` turned out to be. Dependent code
> means the node *kind* itself is provisional.
