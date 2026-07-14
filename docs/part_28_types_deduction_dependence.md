# Part 28 — Types IV: Deduction & Dependence

[← Part 27 — Function, Tag & Sugar](part_27_types_function_tag_sugar.md) | [Part 29 — TypeLoc →](part_29_types_typeloc.md)

Two families remain, and they share a theme: types whose identity is
*computed*. Deduced types (`auto`, CTAD, `decltype`) get their answer within
the translation unit and keep the question as sugar. Dependent types —
everything mentioning a template parameter — stay unanswered inside the
pattern and only resolve per instantiation, the type-level face of the
two-level model from Part 7. This part covers all sixteen remaining kinds,
completing the 59.

## 28.1 — `auto` and CTAD

- **`AutoType`** — an `auto` / `auto&` / `decltype(auto)` / constrained
  `Sortable auto` spelling. Before deduction it is undeduced and dependent;
  after, it wraps the deduced type as sugar. The sample's
  `auto doubled = [](double v) { … };` deduces to the closure type — the
  one-line dump prints just `'(lambda …)'`, but the sugar survives:
  `varDecl(hasType(autoType()))` still matches in clang-query (verified),
  which is exactly what a "ban `auto` here" style check hangs on to.
  Accessors: `getDeducedType()` (null if undeduced), `isConstrained()`,
  `getTypeConstraintConcept()`.
- **`DeducedTemplateSpecializationType`** — class template argument
  deduction: `Pair p{3};` carries this node while `Pair<int>` is being
  chosen (its deduction guides were Part 5's `CXXDeductionGuideDecl`).
  Shares the `DeducedType` base with `AutoType`; same
  `getDeducedType()` idiom.

For analysis, predicates and `getAs<>()` look through deduction sugar
automatically — `auto n = 42;` satisfies `isIntegerType()`. Match the sugar
node only when the check is *about the spelling*.

## 28.2 — `decltype` and pack indexing

- **`DecltypeType`** — `decltype(a)` (verified: dumps as
  `'decltype(a)':'int'` with the operand expression stored;
  `getUnderlyingExpr()`, `getUnderlyingType()`). Remember the language
  rule the node faithfully preserves: `decltype(x)` and `decltype((x))`
  differ — the extra parens make it a reference type.
- **`PackIndexingType`** — C++26 `T...[0]`: selects one element of a type
  parameter pack by index. Rare today; listed so the inventory is complete
  (its expression twin `PackIndexingExpr` was Part 23).

## 28.3 — Template specialization and parameter types

The types that make templates tick — the first two verified in dumps via
in-template typedefs:

| Node | Meaning |
|------|---------|
| `TemplateTypeParmType` | `T` itself inside the pattern — identified by `depth`/`index` coordinates (`'T'` in dumps), no name lookup involved |
| `TemplateSpecializationType` | `Box<T, Args...>` / `std::vector<geo::Shape*>` *as written* — template + argument list; sugar over the `RecordType` once instantiated, dependent while any argument is |
| `SubstTemplateTypeParmType` | post-instantiation sugar recording "this `int` was `T`" — how `clamp<int>`'s parameters remember their template origin |
| `SubstTemplateTypeParmPackType` | same, for an as-yet-unexpanded pack substitution |
| `SubstBuiltinTemplatePackType` | substitution records for builtin templates (`__make_integer_seq`-style) — internal, listed for completeness |


`TemplateSpecializationType` earns a warning label: after instantiation it
is *sugar*, so `dyn_cast` on the canonical type misses it, and before
instantiation it is *dependent*, so `getAsCXXRecordDecl()` returns null.
Checks about "uses of `std::vector` spelled with these arguments" match the
sugar; checks about "this is really a vector object" go canonical. Getting
this backwards is the most common template-tooling bug in existence.

## 28.4 — Dependent names and packs

Inside an uninstantiated template, some types cannot even be classified
yet (all verified in template-body dumps):

| Node | Example inside `template <typename T, typename... Args>` |
|------|----------------------------------------------------------|
| `DependentNameType` | `typename T::value_type` — might be a type, might not exist; nothing is known until `T` is |
| `PackExpansionType` | `Args...` in a parameter list — one type standing for N |
| `UnresolvedUsingType` | the type of a `UsingShadowDecl` whose target is dependent (`using T::type;` — pairs with Part 9's `UnresolvedUsingTypenameDecl`) |
| `DependentAddressSpaceType`, `DependentSizedArrayType`, `DependentVectorType`, `DependentSizedExtVectorType`, `DependentSizedMatrixType`, `DependentBitIntType` | the dependent variants you met in Part 26 — same shapes, sizes pending |

The master switch is `QT->isDependentType()`. Dependent types answer almost
every question with "unknown": no size, no `getAsCXXRecordDecl()`,
predicates conservatively false. This is the type-level counterpart of
Part 23's dependent expressions, and the practical rule is the same one
Part 7 taught for declarations: **a check that inspects types should skip
the dependent pattern and read the instantiations**, where every
`SubstTemplateTypeParmType` wraps a real type and tells the truth — the
sample's `geo::clamp` pattern has `T value` dependent, while `clamp<int>`'s
copy is a plain `int` with substitution sugar.

**Quiz.** Your check wants to flag comparisons between different enum
types. Inside `template <typename E> bool f(E a, Color b) { return a == b; }`
the comparison's operand types are `E` and `Color`. What does
`isEnumeralType()` return for each, and where should the check actually
fire?

> [!hint]- Hint
> One operand's type is a `TemplateTypeParmType`. What do predicates answer
> on dependent types?

> [!success]- Answer
> `Color` answers true; `E` is dependent, so `isEnumeralType()` is
> conservatively false and the pattern looks innocent. Skip function
> templates' patterns (`FD->isTemplated()`) and run the check on
> instantiations — in `f<Fruit>` the substituted type is a real `EnumType`
> and the mixed-enum comparison is visible.

## 28.5 — The Objective-C types

Four kinds serve Objective-C and appear constantly in macOS SDK dumps even
from C++ tools — recognize and skip:

| Node | Source shape |
|------|--------------|
| `ObjCInterfaceType` | `NSString` (the interface itself) |
| `ObjCObjectType` | `NSString<NSCopying>` — interface plus protocol list |
| `ObjCObjectPointerType` | `NSString *` — the only way ObjC objects are handled |
| `ObjCTypeParamType` | type parameters of ObjC lightweight generics |

Out of scope for this lab, in the inventory on principle: with them, all
59 concrete type kinds of this LLVM have now appeared across Parts 25–28 —
every one either explored with a verified dump or placed in a table with
its source shape and escape hatch. Part 29 adds the last piece of the type
world: where types touch source text.
