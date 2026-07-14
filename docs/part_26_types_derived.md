# Part 26 — Types II: Derived Types

[← Part 25 — QualType & Builtins](part_25_types_qualtype_builtins.md) | [Part 27 — Function, Tag & Sugar →](part_27_types_function_tag_sugar.md)

Every declarator construct — `*`, `&`, `[]`, and their exotic cousins — is a
`Type` node wrapping an inner type. This part walks all of them: the four
pointer/reference kinds, all five array kinds, the SIMD family, and the
specialty types (`_Complex`, `_Atomic`, `_BitInt`, OpenCL pipes). Peeling
these wrappers layer by layer is the daily bread of type-aware checks.

## 26.1 — Pointers and references

| Node | Source shape | Key accessors |
|------|--------------|---------------|
| `PointerType` | `T *` | `getPointeeType()` |
| `LValueReferenceType` | `T &` | `getPointeeType()` |
| `RValueReferenceType` | `T &&` | `getPointeeType()` |
| `BlockPointerType` | `T (^)(...)` — Apple blocks (C extension) | `getPointeeType()`; out of scope but common in macOS SDK headers |

All verified in dumps via the typedef trick from Part 25:
`typedef int &&TRRef;` shows `RValueReferenceType` wrapping `BuiltinType
'int'`.

From the lab sample (`skeletons/visitor-tool/sample/sample.cpp`):

```
const ShapeList &shapes        LValueReferenceType → (const) …ShapeList sugar… → RecordType
geo::Shape *                   PointerType → RecordType
```

Peeling is manual and typed — `QT->getPointeeType()` — and the standard
first move when inspecting parameters is `QT.getNonReferenceType()`,
because of an asymmetry worth memorizing: **an expression never has
reference type**. Declarations do (`shapes` is declared
`const ShapeList &`), but once used in an expression the reference is
already "looked through" — that's the value-category machinery from Part 14
doing its job.

The two reference kinds also answer the collapsing rules:
`Ctx.getLValueReferenceType(QT)` / `getRValueReferenceType`, and
`QT->isReferenceType()` covers both.

## 26.2 — Member pointers

`MemberPointerType` covers both flavors of `T C::*`:

```cpp
typedef int  C::*TMemPtr;    // data member pointer
typedef void (C::*TMemFnPtr)();  // member function pointer
```

Both dump as `MemberPointerType` (verified) — the second wrapping a
`FunctionProtoType`. Accessors: `getPointeeType()` (the `int` or the
function type) and the class it belongs to. Predicates:
`QT->isMemberPointerType()`, `isMemberFunctionPointerType()`,
`isMemberDataPointerType()`. They matter to tools mostly as a "don't treat
this like a normal pointer" case: member pointers don't convert to `void*`,
can't be compared with `<`, and have implementation-defined size (often 2×
a normal pointer for member function pointers — ask `Ctx.getTypeSize`, not
your intuition).

## 26.3 — Arrays: all five kinds

Array types are the one corner where even `const` behaves oddly (qualifiers
sink to the element type), and Clang models each source form distinctly:

| Node | Source shape | Notes |
|------|--------------|-------|
| `ConstantArrayType` | `int a[3]` | `getSize()` is an `APInt`; `getSExtValue()` for the number |
| `IncompleteArrayType` | `extern int a[];` / flexible array member | no size at all |
| `VariableArrayType` | `int v[n]` (C VLA; C++ extension) | size is an *expression* — `getSizeExpr()` |
| `DependentSizedArrayType` | `T a[N]` inside a template | size expr is value-dependent; resolves at instantiation |
| `ArrayParameterType` | `int arr[5]` as a *parameter* under C23 rules | records that the array-ness was spelled, even though it adjusts to a pointer |

`ConstantArrayType` and `IncompleteArrayType` verified in dumps
(`typedef int TCArr[3];` → `ConstantArrayType` … `3`).

Because array types have extra uniquing rules, the context mediates access:
`Ctx.getAsConstantArrayType(QT)`, `Ctx.getAsArrayType(QT)` — these look
through sugar *and* handle the qualifier-sinking; prefer them over
`dyn_cast` on the raw type pointer. The everyday predicates:
`QT->isArrayType()`, `isConstantArrayType()`, and for "give me the element
no matter how deep": `Ctx.getBaseElementType(QT)` (peels nested arrays
`int[2][3]` down to `int`).

Arrays also *decay*: passed to functions or used in expressions they become
pointers, and the AST records that honestly — the parameter's type gets
`DecayedType` sugar (Part 27) and the expression gets an
`ArrayToPointerDecay` implicit cast (Part 20). The five nodes above are the
*declared* forms; the decayed pointer is a different type entirely.

**Quiz.** `void f(int arr[5])` — what is `arr`'s type inside `f`, what does
`sizeof(arr)` yield there, and which two AST artifacts preserve the `5` that
the language throws away?

> [!hint]- Hint
> The language adjusts array parameters to pointers; Clang hates losing
> information.

> [!success]- Answer
> Inside `f`, `arr` is `int *` (so `sizeof(arr)` is `sizeof(int*)`, 8 on
> this target). The `5` survives in the AST twice: the parameter's type is
> `DecayedType` sugar whose original type is `ConstantArrayType [5]`
> (`getOriginalType()`), and the `TypeLoc` (Part 29) still spans the
> `[5]` as written. Warn-on-sizeof-of-decayed-array checks are built
> exactly on this.

## 26.4 — Vectors and matrices

The SIMD corner — you met `VectorType` unavoidably in every dump's preamble
(target builtins), and these are their spellings:

| Node | Source shape |
|------|--------------|
| `VectorType` | `typedef int v4 __attribute__((vector_size(16)));` (GCC style) |
| `ExtVectorType` | `typedef float ev4 __attribute__((ext_vector_type(4)));` (OpenCL style — adds `.xyzw` swizzles, Part 17's `ExtVectorElementExpr`) |
| `ConstantMatrixType` | `typedef float m4x4 __attribute__((matrix_type(4,4)));` (needs `-fenable-matrix`) |
| `DependentVectorType` | vector whose size depends on a template parameter |
| `DependentSizedExtVectorType` | ext-vector, dependent size |
| `DependentSizedMatrixType` | matrix, dependent dimensions |

`VectorType`/`ExtVectorType` verified in dumps. Accessors:
`getElementType()`, `getNumElements()` (matrices: `getNumRows()` /
`getNumColumns()`). Unless you're writing GPU or codec tooling you'll only
ever *filter these out* — `QT->isVectorType()` in a guard clause — but now
they have names.

## 26.5 — Complex, atomic, `_BitInt`, and the rest

The remaining derived kinds, all verified in dumps except where noted:

| Node | Source shape | Notes |
|------|--------------|-------|
| `ComplexType` | `_Complex double` | element via `getElementType()`; C's complex, rare in C++ |
| `AtomicType` | `_Atomic(int)` | `getValueType()`; C11 atomics — distinct from `std::atomic<int>`, which is just a `RecordType`! |
| `BitIntType` | `_BitInt(12)` | `getNumBits()`, `isSigned()`; C23 arbitrary-width integers |
| `DependentBitIntType` | `_BitInt(N)` in a template | width is an expression |
| `DependentAddressSpaceType` | `__attribute__((address_space(N)))` with dependent `N` | GPU templates |
| `PipeType` | OpenCL `pipe int` | out of scope; listed for completeness |

The `std::atomic` vs `_Atomic` distinction in that table is a real trap: a
check for "atomic access" that matches `AtomicType` sees only the C11 form;
`std::atomic<T>` is an ordinary class template specialization
(`TemplateSpecializationType` sugar over `RecordType`, Part 28), and you
find it by name, not by type kind.

With this part, every *structural* way to build a type out of another type
has a name. What's left is the function/tag/sugar world (Part 27), and the
deduction/dependence world (Part 28) — plus one meta-skill: when a dump
surprises you, wrap the mystery type in a `typedef` and dump again; the
node chain prints in full.
