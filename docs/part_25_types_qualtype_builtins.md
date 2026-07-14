# Part 25 — Types I: QualType & Builtins

[← Part 24 — Constant Eval & Misc](part_24_expr_consteval_misc.md) | [Part 26 — Derived Types →](part_26_types_derived.md)

Every expression has a type, every declaration declares something *of* a
type, and half of all real-world check logic ("is this a pointer to a
polymorphic class?") is type interrogation. Clang's type system is its own
node hierarchy with its own rules — most importantly, types are **immutable
and uniqued**: `const int` exists exactly once per `ASTContext`, shared by
every `const int` in the program. That single design decision explains most
of the next five parts. This one covers the wrapper you actually handle
(`QualType`), the qualifier model, and the leaf nodes: `BuiltinType` in its
entirety, plus its sugar sibling `PredefinedSugarType`.

## 25.1 — QualType: a Type* plus qualifiers

The thing you pass around is not `Type*` but `QualType` — a pointer-sized
value bundling a `const Type *` with the fast qualifiers:

```cpp
QualType QT = VD->getType();

QT.isConstQualified();          // was it const?
QT.isVolatileQualified();
QT.isRestrictQualified();
QT.getQualifiers();             // the full Qualifiers set
QT.getUnqualifiedType();        // same type, qualifiers stripped
QT.withConst();                 // add const (returns a new QualType)
QT.isNull();                    // QualType can be empty — check before use
QT.getAsString();               // "const ShapeList &" — for humans
```

Why aren't qualifiers part of the `Type` node? Uniquing. If `const int` and
`int` were different `Type` objects, every qualifier combination would
multiply the type table; instead there is one `BuiltinType` for `int` and
the `const` rides in spare bits of the `QualType` pointer value.

The `->` operator forwards to the underlying `Type`, so you constantly mix
the two levels:

```cpp
QT->isPointerType()       // question about the Type       (arrow)
QT.isConstQualified()     // question about the qualifiers (dot)
```

That dot/arrow split trips everyone once: `QT->isConstQualified()` doesn't
compile, and `QT.isPointerType()` doesn't exist. Qualifier questions use the
dot; type-shape questions use the arrow.

**Quiz.** For `const geo::Shape *p`, does `p`'s `QualType` report
`isConstQualified()` as true?

> [!hint]- Hint
> What exactly is const here — the pointer, or the thing it points to?

> [!success]- Answer
> No. `p` itself is a mutable pointer, so its `QualType` (a `PointerType`,
> no qualifiers) is not const-qualified. The `const` lives on the *pointee*:
> `QT->getPointeeType().isConstQualified()` is true. For `Shape *const p`
> it would be the other way around.

## 25.2 — The full qualifier model

`const`/`volatile`/`restrict` are the *fast* qualifiers — three bits in the
pointer. The `Qualifiers` class carries more, for the language extensions
that need them:

| Qualifier group | Source | Where you'd meet it |
|-----------------|--------|---------------------|
| CVR | `const`, `volatile`, `restrict` | everywhere |
| Address space | `__attribute__((address_space(N)))`, OpenCL `__global`/`__local`… | GPU/embedded tooling; `Q.getAddressSpace()` |
| ObjC GC / lifetime | `__strong`, `__weak`, `__autoreleasing` (ARC) | Objective-C only — listed for completeness, out of scope here |
| Unaligned | MS `__unaligned` | Windows codebases |

When a type carries non-fast qualifiers, Clang packs them into an
`ExtQuals` node behind the scenes — invisible unless you go looking, but
it's why `Qualifiers` is its own class rather than three bools. The
practical API surface stays small: `QT.getQualifiers()`, `Qualifiers::empty()`,
`Q.hasConst()`, `Q.getAddressSpace()`.

One subtlety worth internalizing early: **qualifiers on the top level of a
type are different from qualifiers buried inside it.** `const int *` is an
unqualified `PointerType` pointing at qualified `int`; `int *const` is a
const-qualified `PointerType`. `QT.getUnqualifiedType()` only strips the
top; nothing standard strips them recursively (checks that need "same type
modulo all qualifiers" use `Ctx.hasSameUnqualifiedType()` — Part 27).

## 25.3 — BuiltinType: the complete zoo

The leaves of the type world. One node class, discriminated by
`BuiltinType::getKind()` — and this is the complete C/C++-relevant kind
list:

| Kind group | Kinds |
|------------|-------|
| void | `Void` |
| boolean | `Bool` |
| narrow character | `Char_S` / `Char_U` (plain `char` — signedness picked per target, a distinct type from both below), `SChar`, `UChar` |
| wide/unicode character | `WChar_S` / `WChar_U` (plain `wchar_t`), `Char8`, `Char16`, `Char32` |
| signed integer | `Short`, `Int`, `Long`, `LongLong`, `Int128` |
| unsigned integer | `UShort`, `UInt`, `ULong`, `ULongLong`, `UInt128` |
| floating | `Half`, `Float`, `Double`, `LongDouble`, `Float16`, `BFloat16`, `Float128`, `Ibm128` |
| nullptr | `NullPtr` — the type of `nullptr` (`std::nullptr_t`; verified: `typedef decltype(nullptr) T;` dumps `BuiltinType 'std::nullptr_t'`) |
| C-only | `_Accum`/`_Fract` fixed-point families (embedded C), `_Sat` variants |
| placeholders | `Dependent`, `Overload`, `BoundMember`, `UnknownAny`, `BuiltinFn`, `PseudoObject`, `ARCUnbridgedCast` — internal states an expression's type can be in *mid-analysis*; you'll see `Overload` as the type of an unresolved overload set (Part 23) |
| target-specific | OpenCL image/sampler/event kinds, SVE/NEON/RVV vector kinds, AMX tile kinds, WebAssembly reference kinds |

Three practical notes. First, platform honesty: plain `char` is a *distinct
kind* from `signed char` and `unsigned char` — three different types in
C++, and the AST preserves that; a matcher for `isAnyCharacterType()` exists
precisely because of it. Second, the `ASTContext` keeps ready-made
`QualType`s for all of these — `Ctx.IntTy`, `Ctx.BoolTy`, `Ctx.DoubleTy`,
`Ctx.VoidTy`, `Ctx.Char8Ty`, … — which is what you compare against or
construct with. Third, the placeholder kinds are the reason
`Expr::getType()` is not always a "real" type: before overload resolution
completes, the callee of an ambiguous call has type `Overload`, and code
that blindly does `getType()->isFunctionType()` misfires; `Expr` has
`hasPlaceholderType()` for exactly this check.

Size questions go through the context (only it knows the target), and the
unit is **bits**:

```cpp
Ctx.getTypeSize(QT);          // int → 32, bool → 8, double → 64 (BITS)
Ctx.getTypeAlign(QT);         // also bits
Ctx.getTypeSizeInChars(QT);   // in bytes, when that's what you meant
```

The bits-not-bytes convention is a classic first-tool bug — a check that
warns "field larger than 64" fires on every `long` unless you meant bits.
(Verified against this lab's LLVM: `Ctx.getTypeSize(Ctx.IntTy)` is 32,
`BoolTy` is 8.)

## 25.4 — PredefinedSugarType

The one sugar node that lives at the builtin level: `PredefinedSugarType`
wraps a builtin under a compiler-blessed name — in C modes,
`size_t`/`ptrdiff_t`-style names produced by the implementation itself
(rather than through a header's `typedef`) print this way. You will
essentially never match on it; it's listed here because it *is* one of the
59 concrete type kinds and you may spot it in dumps of freestanding C code.
When you need "is this `size_t`?" semantics, compare against
`Ctx.getSizeType()` with `hasSameType` and sugar becomes irrelevant.

A dump-reading habit to start now, because it pays off through Part 29:
type node kinds mostly *don't* get their own dump lines under variables —
a `VarDecl` line just prints the type in quotes. The trick to see the node
structure is to dump a `typedef` of the type; `TypedefDecl` prints its
complete underlying type tree. That trick produced every verified claim in
Parts 25–28:

```bash
echo 'typedef decltype(nullptr) TNull;' > /tmp/t.cpp
$LLVM/bin/clang++ -std=c++17 -Xclang -ast-dump -fsyntax-only /tmp/t.cpp
# TypedefDecl … TNull 'decltype(nullptr)':'std::nullptr_t'
# `-DecltypeType … 'decltype(nullptr)' sugar
#   `-BuiltinType … 'std::nullptr_t'
```

**Quiz.** `Ctx.getTypeSize(Ctx.BoolTy)` returns 8, yet
`std::vector<bool>` packs booleans into single bits, and `sizeof(bool)` is
1. Which of the three numbers does the AST-level answer correspond to, and
why doesn't the AST know about the packing?

> [!hint]- Hint
> One of the three is in bits, one is in bytes, and one isn't a property of
> the *type* at all.

> [!success]- Answer
> `getTypeSize` is `sizeof` in bits: 8 bits = the 1 byte that `sizeof(bool)`
> reports. `vector<bool>`'s packing is a library implementation detail — a
> data-structure layout choice, not a property of the type `bool` — so no
> type-level API reflects it. Type sizes come from the target ABI;
> container layouts come from code.
