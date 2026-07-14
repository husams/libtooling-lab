# Part 9 — Declarations VII: Enums, Aliases & Scopes

[← Part 8](part_8_decls_templates_classes.md) | [Part 10 — The Long Tail →](part_10_decls_long_tail.md)

Fifteen declaration kinds organize names rather than define behavior:
enums and their constants, both spellings of type aliasing, namespaces and
their aliases, and the eight-strong `using` family. Individually small,
collectively they're most of what makes C++ name lookup interesting — and
most of what reference-tracking tools get subtly wrong.

## 9.1 — EnumDecl

Like `CXXRecordDecl`, `EnumDecl` is a `TagDecl` — same
forward-declaration/definition mechanics, same context behavior. The
sample's enum dumps as:

```
EnumDecl … referenced class Color 'int'
├─EnumConstantDecl … Red 'Color'
├─EnumConstantDecl … Green 'Color'
└─EnumConstantDecl … referenced Blue 'Color'
```

The `class` and the `'int'` in that first line are the two axes the API
exposes:

```cpp
ED->isScoped();                  // enum class / enum struct
ED->isScopedUsingClassTag();     // 'class' vs 'struct' spelling
ED->getIntegerType();            // underlying type — 'int' here (fixed or deduced)
ED->isFixed();                   // was ': int' written?
ED->getPromotionType();          // what it becomes in arithmetic
ED->enumerators();               // the EnumConstantDecl range
```

## 9.2 — EnumConstantDecl

Each enumerator is a `ValueDecl` (expressions refer to it) but *not* a
`DeclaratorDecl` — Part 3's quiz distinction:

```cpp
ECD->getInitVal();               // llvm::APSInt — value ALWAYS available
ECD->getInitExpr();              // Expr* — null unless '= expr' was typed
```

Two things trip tools up. First, enumerator *values* are always available
via `getInitVal()` even when no initializer was written — Sema computed
them — while `getInitExpr()` is null unless one was typed. Second, scoping:
in a scoped enum the constants' `DeclContext` is the enum itself
(`Color::Blue` must be qualified), while unscoped enumerators are *also*
visible in the enclosing scope — same nodes, richer lookup, no duplicates.

## 9.3 — TypedefDecl / TypeAliasDecl

Both spellings of "name this type" — `TypedefDecl` and `TypeAliasDecl` —
derive from the abstract `TypedefNameDecl`:

```cpp
using ShapeList = std::vector<geo::Shape *>;   // TypeAliasDecl
typedef double scalar_t;                        // TypedefDecl
```

```
TypeAliasDecl … referenced ShapeList 'std::vector<geo::Shape *>'
└─TemplateSpecializationType … 'std::vector<geo::Shape *>' sugar
```

The API is small — `getUnderlyingType()` is the whole story
(`ShapeList` → `std::vector<geo::Shape *>`) — but the *consequences* are
large: every use of `ShapeList` in the program carries the alias as **type
sugar** rather than being replaced by it, which is why Part 2's
`totalArea` dump said `const ShapeList &` and not the vector type. The
decl is where the name is born; Part 27 covers how it echoes through the
type system as `TypedefType`, and §8.5 covered the templated big brother.

## 9.4 — NamespaceDecl & NamespaceAliasDecl

Namespaces are pure `DeclContext`s with three variations worth testing
for:

```cpp
NS->isAnonymousNamespace();   // namespace { … }  — internal linkage for members
NS->isInline();               // inline namespace lib { … } — members visible in parent
NS->getNameAsString();        // "" for anonymous — getNameAsString, as always
NS->isNested();               // namespace a::b — each level its own node
```

A namespace reopened five times produces five `NamespaceDecl` nodes — a
redeclaration chain exactly like Part 3 §3.4, so canonicalize before
using one as a map key. **`NamespaceAliasDecl`**
(`namespace fs = std::filesystem;`) completes the pair;
`getNamespace()` resolves to the real one, and `DeclRefExpr` qualifiers
spell whichever the user wrote.

## 9.5 — Using declarations & their shadows

The core mechanism takes four kinds. `using std::swap;` creates one
**`UsingDecl`** ("import this name here") — and, per entity the name
resolves to, one **`UsingShadowDecl`**, the invisible alias that lookup
actually finds:

```cpp
UD->shadows();                   // the UsingShadowDecls this using-decl spawned
USD->getTargetDecl();            // the real entity behind the shadow
USD->getIntroducer();            // back to the UsingDecl
```

After `using std::swap;`, a `DeclRefExpr` to `swap` may point at the
`UsingShadowDecl`, and honest reference-tracking follows
`getTargetDecl()` to reach the entity — rename tools that skip this hop
rename the wrong declaration. **`ConstructorUsingShadowDecl`** is the same
idea for inherited constructors (`using Base::Base;`), and
**`UsingDirectiveDecl`** (`using namespace std;`) is the bulk variant:
it creates *no* shadows at all, just makes a whole namespace visible
(`getNominatedNamespace()`).

**Quiz.** `-ast-dump` of a file containing `using namespace geo;` followed
by `Circle c({0,0}, 1);` — which nodes from this section appear, and
which one does the type reference in `Circle c` actually go through?

> [!hint]- Hint
> Directives don't create per-name nodes. Shadows do — but only for
> `using` *declarations*.

> [!success]- Answer
> Only a `UsingDirectiveDecl` appears (nominating `geo`) — a directive
> imports nothing by name, so there are no `UsingShadowDecl`s, and the
> reference to `Circle` resolves *directly* to `geo::Circle`'s
> `CXXRecordDecl`. Shadows would appear only for `using geo::Circle;`.
> Lookup behavior differs the same way — which is why the two spellings
> are distinct node kinds instead of one "using" node.

## 9.6 — The C++20 & dependent corners

Four more kinds finish the family. **`UsingEnumDecl`**
(`using enum Color;`) imports all enumerators at once — shadows appear for
each, same mechanism as §9.5.

The dependent trio exists because inside a template, `using Base<T>::member;`
can't know what it imports until instantiation:

| Kind | Source | Resolves to |
|------|--------|-------------|
| `UnresolvedUsingValueDecl` | `using Base<T>::member;` (value) | a shadow per found entity, at instantiation |
| `UnresolvedUsingTypenameDecl` | `using typename Base<T>::type;` | a type alias, at instantiation |
| `UnresolvedUsingIfExistsDecl` | `using __if_exists`-style library machinery | possibly nothing — the "if exists" is literal |

And the pack expansion case — the C++17 `overloaded` idiom — shows the
whole pipeline at once. Verified dump of
`template <class... Ts> struct overloaded : Ts... { using Ts::operator()...; };`
instantiated as `overloaded<A, B>`:

```
(pattern)        UnresolvedUsingValueDecl … Ts::operator() '<dependent type>'
(instantiation)  UsingPackDecl …
                 ├─UsingShadowDecl … implicit CXXMethod 'operator()' 'void (int) const'
                 └─UsingShadowDecl … implicit CXXMethod 'operator()' 'void (double) const'
```

**`UsingPackDecl`** is the instantiation-side record of the expanded pack —
`expansions()` lists the per-element using declarations, whose shadows
finally point at real `operator()`s. Fifteen kinds, one mechanism: a name
is imported, a shadow records what it resolved to, and everything a tool
needs is one `getTargetDecl()` away.
