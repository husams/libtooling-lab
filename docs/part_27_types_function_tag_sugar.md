# Part 27 — Types III: Function, Tag & Sugar Types

[← Part 26 — Derived Types](part_26_types_derived.md) | [Part 28 — Deduction & Dependence →](part_28_types_deduction_dependence.md)

Three worlds meet in this part. Function types carry the richest per-node
payload in the type system (parameters, exception specs, qualifiers on
methods). Tag types are the bridges back to the `Decl` hierarchy you toured
in Parts 6 and 9. And sugar types — the largest *category* of type nodes —
exist purely to remember how a type was written, culminating in the
canonical-type rules every non-toy tool depends on.

## 27.1 — Function types

| Node | Source shape |
|------|--------------|
| `FunctionProtoType` | `double(const ShapeList &)` — any function type with a parameter list |
| `FunctionNoProtoType` | K&R C `void oldstyle();` — no parameter info at all (C17 and earlier; gone in C23) |

`FunctionProtoType` (verified in dumps under `typedef int TFn(int, ...);`)
is where a signature's semantics live:

```cpp
const auto *FPT = QT->getAs<FunctionProtoType>();
FPT->getReturnType();
FPT->param_types();              // ArrayRef<QualType>
FPT->getNumParams();
FPT->isVariadic();               // the ... (verified: dump prints 'variadic')
FPT->getMethodQuals();           // const/volatile on a member function type
FPT->getRefQualifier();          // & / && qualifiers (RQ_LValue / RQ_RValue)
FPT->getExceptionSpecType();     // EST_BasicNoexcept, EST_DependentNoexcept, …
FPT->isNothrow(Ctx);             // the question you usually mean
```

Two facts that surprise people. First, the *method* qualifiers ride on the
function type, not on `CXXMethodDecl` — `void f() const` has type
`void () const`, a different `FunctionProtoType` from `void ()`. Second,
parameter *names* are not in the type at all (they're `ParmVarDecl`s,
Part 4); the type only knows parameter types. Two declarations with
different parameter names have `hasSameType` — which is exactly right.

## 27.2 — Tag types and the injected class name

A `RecordType` / `EnumType` is a thin wrapper whose whole job is pointing at
the declaration you already know:

```cpp
if (const auto *RT = QT->getAs<RecordType>()) {
  RecordDecl *RD = RT->getDecl();          // back to the Decl hierarchy
}

// The everyday shortcut — walks through sugar AND checks it's a class:
if (CXXRecordDecl *RD = QT->getAsCXXRecordDecl()) { … }
```

`getAs<T>()` (on `Type`) is the sugar-aware `dyn_cast` for types: it
desugars step by step until it finds a `RecordType` or gives up — so it
works on `ShapeList` even though the top node is an alias. Plain
`dyn_cast<RecordType>(QT.getTypePtr())` fails there; use `getAs`.

Version note (LLVM 22, this lab's toolchain — verified): older Clang
wrapped qualified spellings (`geo::Shape`) and elaborated spellings
(`struct S`) in an `ElaboratedType` sugar node. **LLVM 22 removed it.**
`typedef struct C TRec;` now dumps a bare `RecordType`, with any
`NestedNameSpecifier` information carried by the node itself. Old matcher
recipes using `elaboratedType()` have no node left to match.

The third tag-family member is `InjectedClassNameType` (verified in
template dumps): inside `class Circle`, the bare name `Circle` is usable as
a type; inside a template `Box<T, Args...>`, the injected name `Box` means
"this very instantiation" without spelling the arguments. It matters mainly
when you compare types inside a class template against the template itself.

## 27.3 — Sugar nodes: the complete set

Sugar exists so diagnostics can say what the user wrote. Each node answers
`desugar()` (one layer off) and prints in dumps as a
`'sugar':'canonical'` pair — `'ShapeList':'std::vector<geo::Shape *>'`.

| Sugar node | What was written | Verified |
|------------|------------------|----------|
| `TypedefType` | `MyInt` / `ShapeList` (both `typedef` and `using` aliases) | ✓ sample dumps |
| `UsingType` | a type named through a `using` *declaration* (`using N::S; S s;`) | ✓ |
| `ParenType` | the parentheses in `int (*f)()` | ✓ |
| `MacroQualifiedType` | a type spelled through a qualifier-adding macro (`#define NONNULL _Nonnull`) | — |
| `AttributedType` | `int * _Nonnull`, `[[clang::annotate_type(...)]]` | — |
| `BTFTagAttributedType` | `__attribute__((btf_type_tag("...")))` (Linux/BPF) | listed |
| `CountAttributedType` | `__counted_by(n)` bounds-safety annotations | listed |
| `AdjustedType` | silent parameter adjustments | — |
| `DecayedType` (an `AdjustedType` subclass) | array/function parameter decayed to pointer; `getOriginalType()` | Part 26 quiz |
| `PredefinedSugarType` | implementation-provided names (`size_t` in freestanding C) | Part 25 |

Sugar is *transparent* to every predicate and to `getAs<>()`:
`ShapeList`-typed values answer `isRecordType()` true. It is *opaque* only
to code that looks at the node kind directly — which is sometimes exactly
what a check wants ("flag uses of the deprecated alias `LegacyHandle`"
matches `TypedefType` by name, canonical identity be damned).

## 27.4 — `typeof` and `__underlying_type`

Three C-flavored computed-type nodes complete the sugar family (all
verified in dumps):

- **`TypeOfExprType`** — C23/GNU `typeof(1 + 1)`: stores the operand
  *expression*; `getUnderlyingExpr()`.
- **`TypeOfType`** — `typeof(int)`: the type-operand form.
- **`UnaryTransformType`** — `__underlying_type(E)` (the builtin beneath
  `std::underlying_type`): dumps show it wrapping the enum's fixed
  underlying type; `getUnderlyingType()`, `getUTTKind()`.

Their C++ cousin `decltype` lives in Part 28 with the deduction family —
same idea, different scoping rules.

## 27.5 — Canonical types: the rules that keep tools honest

Every `QualType` knows its fully-desugared self:

```cpp
QT.getCanonicalType()      // all sugar removed, uniqued — safe to compare
Ctx.hasSameType(A, B)      // compares canonical types for you
Ctx.hasSameUnqualifiedType(A, B)   // …ignoring top-level const/volatile too
```

Uniquing (Part 25) applies to *canonical* types only: there is exactly one
canonical `std::vector<geo::Shape *>` per context, but any number of
distinct sugar chains pointing at it. Hence the two iron rules:

- **Identity questions** → canonical: `Ctx.hasSameType(...)`, or pointer
  equality of `getCanonicalType().getTypePtr()`.
- **Display** → sugared: `QT.getAsString()` shows what the user wrote,
  which is what they want to read in a diagnostic.

The classic bug this prevents: a check for functions returning
`std::size_t` written as a string comparison misses every platform where
`size_t` unwinds through different sugar — while
`hasSameType(FD->getReturnType(), Ctx.getSizeType())` never misses. Never
compare `getAsString()` output; it is for eyes only.

**Quiz.** `QT.getAsString()` for the sample's `shapes` parameter prints
`const ShapeList &`. What does
`QT.getCanonicalType().getAsString()` print?

> [!hint]- Hint
> Desugaring goes through the alias *and* keeps qualifiers and the
> reference.

> [!success]- Answer
> `const std::vector<geo::Shape *> &` — sugar is removed (alias unwound,
> namespace spelled), but constness and reference-ness are structure, not
> sugar; they survive canonicalization.

Out-of-scope but on the books, for the completeness this lab promises:
`HLSLAttributedResourceType` and `HLSLInlineSpirvType` (HLSL shader
tooling) round out the attributed-sugar corner. If you ever meet them, the
pattern is the same: sugar wrapping a real type, `desugar()` to pass
through.
