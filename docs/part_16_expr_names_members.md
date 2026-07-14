# Part 16 — Expressions: Names & Members

[← Part 15 — Literals](part_15_expr_literals.md) | [Part 17 — Operators →](part_17_expr_operators.md)

Using a name is an expression, and Clang resolves every use to its
declaration *during parsing* — the AST hands you a finished cross-reference
graph. The two nodes that carry it, `DeclRefExpr` and `MemberExpr`, are the
most-visited expression kinds in real tools; this part covers them in full,
plus the "magic identifier" nodes and the Microsoft/vector tail.

## 16.1 — DeclRefExpr

**`DeclRefExpr`** is a use of a named entity — and its superpower is the
resolved edge back into the `Decl` world:

```cpp
DRE->getDecl();        // the ValueDecl this name refers to — no lookup needed
DRE->getFoundDecl();   // what name lookup found (differs via using-decls)
DRE->hasQualifier();   // was it written geo::kPi rather than kPi?
DRE->getQualifier();   // the NestedNameSpecifier for geo::
DRE->getNameInfo().getLoc();     // location of the name itself
DRE->getTemplateArgs();          // f<int>(…) — explicit template args, with locations
DRE->hasExplicitTemplateArgs();
```

`getDecl()` is the single most-used API in Clang tooling: from any use of
`total`, one call lands on the `VarDecl` with the initializer, type, and
location. Uses and declarations form a graph you never have to build.

The `getDecl()` / `getFoundDecl()` split is Part 9's `using` machinery
surfacing here: after `using geo::clamp;`, a call to `clamp` *finds* the
`UsingShadowDecl` but *refers to* the original `FunctionDecl`. Tools
enforcing "don't bypass the facade header" live entirely on that
distinction.

Note also what `DeclRefExpr` refuses to be: it never refers to a member
accessed through an object — that's `MemberExpr` — and inside templates an
unresolvable name is a different node kind entirely (Part 23's
`DependentScopeDeclRefExpr` / `UnresolvedLookupExpr`). When you match
`declRefExpr()` you are matching *resolved, non-member* uses only.

## 16.2 — MemberExpr

**`MemberExpr`** is `obj.field` / `ptr->method`:

```cpp
ME->getBase();          // the object expression
ME->isArrow();          // -> vs .
ME->getMemberDecl();    // FieldDecl or CXXMethodDecl (a ValueDecl)
ME->getFoundDecl();     // same using-decl story as DeclRefExpr
ME->isImplicitAccess(); // true for bare `radius_` inside a method
ME->getQualifier();     // obj.Base::f — explicit qualification
ME->getMemberLoc();     // location of the member name
```

In `totalArea`'s loop body, `shape->area()` contains a `MemberExpr` with
`isArrow() == true` whose member is the *virtual* `CXXMethodDecl
Shape::area` — the static target; whether the virtual call dispatches
elsewhere at runtime is exactly what the AST cannot know.

`isImplicitAccess()` deserves respect: inside `Circle::area()`, the bare
`radius_` is a `MemberExpr` whose base is an *implicit* `CXXThisExpr` — the
tree contains `this->radius_` even though the source doesn't. A rename
tool that blindly prepends text at `getBase()`'s location corrupts exactly
these nodes; check the flag first, and note `getBeginLoc()` of an implicit
access starts at the member name itself.

## 16.3 — PredefinedExpr

`__func__` (and the GNU spellings `__FUNCTION__`, `__PRETTY_FUNCTION__`)
are not macros — the preprocessor can't know the function name. They're a
dedicated node, **`PredefinedExpr`** (verified dump: `PredefinedExpr 'const
char[4]' lvalue __func__`):

```cpp
PE->getIdentKind();      // PredefinedIdentKind::Func / Function / PrettyFunction…
PE->getFunctionName();   // the synthesized StringLiteral (null in templates
                         //   before instantiation — dependent length!)
```

That parenthetical is a real trap: in a template pattern the string's
length depends on the instantiation, so `getFunctionName()` is null there
and materializes per specialization.

## 16.4 — SourceLocExpr

The C++20-era source-location builtins — `__builtin_LINE()`,
`__builtin_FUNCTION()`, `__builtin_FILE()`, `__builtin_COLUMN()`,
`__builtin_source_location()` (the engine under
`std::source_location::current()`) — get their own node (verified:
`SourceLocExpr`), and it has the most interesting evaluation rule in this
part:

```cpp
SLE->getIdentKind();       // SourceLocIdentKind::Line / File / Function / SourceLocStruct…
SLE->getParentContext();   // where a defaulted-argument use will evaluate
```

A `SourceLocExpr` in a **default argument** evaluates at the *call site*,
not the declaration — that's its entire purpose (`void log(int line =
__builtin_LINE())`). So its value isn't a property of the node but of each
use; constant-folding it (Part 24) needs the surrounding context, which is
why `getParentContext()` exists.

## 16.5 — The Microsoft and vector tail

The remaining name/member-access kinds, complete:

| Node | Source shape | Notes |
|------|--------------|-------|
| `MSPropertyRefExpr` | `obj.Prop` where `Prop` is `__declspec(property(get=…, put=…))` | MSVC property sugar; reads/writes become getter/setter calls at CodeGen, but the AST keeps the property reference |
| `MSPropertySubscriptExpr` | `obj.Prop[i]` on an indexed MSVC property | the subscript half of the same sugar |
| `ExtVectorElementExpr` | `v.xyzw`, `v.lo` on OpenCL/ext_vector types | element swizzles; `getAccessor()` names the components, `containsDuplicateElements()` matters for lvalue-ness |

All three require dialect flags (`-fms-extensions`, vector type
extensions); they're listed so no dump ever shows you an unidentifiable
member access.

**Quiz.** Inside `Circle::area()`, the sample computes `kPi *
SQUARE(radius_)`. For the two leaf name-uses — `kPi` and `radius_` — name
the node kind of each use and of its base (if any), and say which of the
two would break if `area()` were made `static`.

> [!hint]- Hint
> One is a namespace-scope `constexpr` variable; the other is a non-static
> data member with an invisible companion.

> [!success]- Answer
> `kPi` is a `DeclRefExpr` (with no qualifier — it's found by unqualified
> lookup inside `geo`), no base. `radius_` is a `MemberExpr` with
> `isImplicitAccess() == true`, its base an implicit `CXXThisExpr`. Making
> `area()` static removes `this`, so the `radius_` access becomes
> ill-formed — the `MemberExpr`'s implicit base is the dependency; `kPi`
> is untouched.
