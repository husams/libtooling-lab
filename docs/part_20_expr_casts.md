# Part 20 — Expressions: Casts

[← Part 19 — Construction & Init](part_19_expr_construction_init.md) | [Part 21 — C++ Objects →](part_21_expr_cpp_objects.md)

Every conversion in a C++ program — written or silent — is a node in the
AST, and they all share one machinery: the `CastExpr` base, a sub-expression,
and a **`CastKind`** saying exactly which conversion happened. Master the
kind taxonomy once and every dump you ever read becomes self-explanatory.

## 20.1 — The CastExpr machinery

```
CastExpr
├─ ImplicitCastExpr                  the compiler inserted it
└─ ExplicitCastExpr                  you wrote something
   ├─ CStyleCastExpr                 (T)e
   ├─ CXXFunctionalCastExpr          T(e)
   ├─ CXXNamedCastExpr
   │  ├─ CXXStaticCastExpr           static_cast<T>(e)
   │  ├─ CXXDynamicCastExpr          dynamic_cast<T>(e)
   │  ├─ CXXReinterpretCastExpr      reinterpret_cast<T>(e)
   │  ├─ CXXConstCastExpr            const_cast<T>(e)
   │  └─ CXXAddrspaceCastExpr        addrspace_cast<T>(e)   (OpenCL)
   ├─ BuiltinBitCastExpr             __builtin_bit_cast(T, e)
   └─ ObjCBridgedCastExpr            (__bridge T)e           (ObjC ARC)
```

Shared API, whichever node you hold:

```cpp
CE->getCastKind();          // CK_LValueToRValue, CK_IntegralCast, …
CE->getCastKindName();      // "LValueToRValue" — the string in the dump
CE->getSubExpr();           // what's being converted
CE->getSubExprAsWritten();  // peels intermediate implicit steps
CE->path();                 // for base-class casts: the inheritance path
```

## 20.2 — ImplicitCastExpr and the CastKind taxonomy

There are ~70 `CastKind`s; a dozen account for nearly everything you'll see:

| CastKind | Meaning | Where you've already seen it |
|----------|---------|------------------------------|
| `CK_LValueToRValue` | read the value out of a location | around every variable *use* |
| `CK_FunctionToPointerDecay` | function name → function pointer | every call's callee (Part 18) |
| `CK_ArrayToPointerDecay` | array → pointer to first element | `"circle"` passed to `std::string` |
| `CK_IntegralCast` | int ↔ wider/narrower int | `geo::clamp(42, 0, 10)` args if types differ |
| `CK_FloatingCast` | float ↔ double … | mixed float arithmetic |
| `CK_IntegralToFloating` / `CK_FloatingToIntegral` | numeric family change | `2 * v` when `2` meets `double` |
| `CK_DerivedToBase` | `Circle*` → `Shape*` | `shapes{&unit}` in the sample |
| `CK_BaseToDerived` | the reverse — only via explicit casts | `static_cast<Circle*>(shape)` |
| `CK_NullToPointer` | `nullptr`/`0` → pointer type | `Shape *s = nullptr;` |
| `CK_NoOp` | type adjustment, no value change (adding `const`) | binding `const T&` to `T` |
| `CK_BitCast` | pointer reinterpretation | most `reinterpret_cast`s |
| `CK_ConstructorConversion` | implicit conversion via converting ctor | `Shape("circle")`'s `const char*` → `std::string` |
| `CK_UserDefinedConversion` | via `operator T()` | lambda → function pointer |
| `CK_ToVoid` | discard a value | `(void)result;` |
| `CK_IntegralToBoolean` / `CK_PointerToBoolean` | condition contexts | every `if (p)` |
| `CK_Dynamic` | the runtime-checked conversion | `dynamic_cast` only |

Two mental anchors. First: **the dump never lies about conversions** — if
arithmetic mixes types, the exact promotion path is spelled out as nested
casts, which is why integer-promotion bug hunts are tractable as AST checks.
Second: `CK_NoOp` is not "nothing happened" — the *value* is unchanged but
the type (usually qualification) changed; rewriting tools that drop NoOp
casts can change overload resolution.

## 20.3 — The explicit casts, one by one

**`CStyleCastExpr`** — `(T)e`, including the `(void)result;` lines in
`main`. The chameleon: its `getCastKind()` reveals which named cast it
actually performs, which is exactly how a modernizer decides the
replacement. `getTypeInfoAsWritten()` gives the parenthesized type with its
source location — the piece you'd rewrite.

**`CXXFunctionalCastExpr`** — `T(e)` with exactly one argument. With more
arguments you get `CXXTemporaryObjectExpr` instead (Part 19); zero arguments
give `CXXScalarValueInitExpr` for scalars. Three syntaxes that look like
"constructor calls", three different nodes — match all three when hunting
"conversion-style" code.

**`CXXStaticCastExpr`** — the workhorse; kinds range from `CK_NoOp` through
`CK_DerivedToBase`, `CK_IntegralCast`, `CK_BaseToDerived` (the unchecked
downcast).

**`CXXDynamicCastExpr`** — the only cast with a runtime component
(`CK_Dynamic`). `isAlwaysNull()` is a fun accessor: Clang statically proves
some dynamic_casts dead.

**`CXXReinterpretCastExpr`** — almost always `CK_BitCast` or
`CK_IntegralToPointer`/`CK_PointerToIntegral`. A favorite matcher target for
security linters.

**`CXXConstCastExpr`** — always `CK_NoOp`: qualification is the only thing
that changes, which is precisely why it deserves its own audit trail in
reviews.

**`BuiltinBitCastExpr`** — `__builtin_bit_cast(int, 1.0f)` (what
`std::bit_cast` lowers to; verified in the C++20 probe). Object
representation copy, `CK_LValueToRValue` under the hood, constexpr-capable —
the sanctioned replacement for the `reinterpret_cast`-through-pointer UB
pattern.

The named four share the `CXXNamedCastExpr` base:

```cpp
NC->getCastName();               // "static_cast" …
NC->getAngleBrackets();          // SourceRange of <T> — rewriting gold
NC->getTypeInfoAsWritten();      // the T, with location
```

`CXXAddrspaceCastExpr` (OpenCL address spaces) and `ObjCBridgedCastExpr`
(ARC bridging) complete the inventory — you'll likely never match either in
C++ code, but now no dump line can surprise you.

## 20.4 — A worked modernizer

Everything above compresses into the classic check "replace C-style casts":

```cpp
// matcher
cStyleCastExpr(unless(isExpansionInSystemHeader())).bind("cast")

// callback logic
const auto *CSC = Result.Nodes.getNodeAs<CStyleCastExpr>("cast");
switch (CSC->getCastKind()) {
case CK_NoOp:            // qualification-only → const_cast or static_cast
case CK_IntegralCast:
case CK_FloatingCast:    // value conversions   → static_cast
case CK_DerivedToBase:   //                     → static_cast
  … suggest static_cast …
case CK_BitCast:         // representation      → reinterpret_cast
  … suggest reinterpret_cast, or flag for review …
case CK_ToVoid:          // (void)x idiom       → leave alone
  return;
default: …
}
```

The cast kind does the semantic analysis for you; the tool only maps kinds
to spellings. This division — Clang classifies, you police style — is the
recurring shape of cast-related tooling.

**Quiz.** The sample's `total += shape->area()` dump contains exactly **one**
`ImplicitCastExpr` — `<LValueToRValue>` on `shape`. Two absences want
explaining: why does `total` (also just a named variable) get no load cast,
and why is there no `CK_FunctionToPointerDecay` on the callee the way every
free-function call has one?

> [!hint]- Hint
> What does `+=` do to its left side that plain `+` doesn't? And can you
> even *form* a pointer to `area`  with just its name?

> [!success]- Answer
> `+=` needs `total` as a **location** to write into, so the LHS stays an
> lvalue — no `LValueToRValue` (the dump's `ComputeLHSTy` records the
> read-modify-write typing instead). And member calls don't decay:
> `->area` is a `MemberExpr` of `'<bound member function type>'`, a special
> placeholder type — bound member functions aren't values, which is also
> why `auto f = shape->area;` is ill-formed. Decay-to-pointer exists only
> for free functions.
