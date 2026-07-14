# Part 8 — Declarations VI: Class, Variable & Alias Templates, Concepts

[← Part 7](part_7_decls_templates_functions.md) | [Part 9 — Enums, Aliases & Scopes →](part_9_decls_enums_scopes.md)

Part 7's two-level model, applied three more times — classes, variables,
aliases — each with its own specialization node kinds, then the C++20
concepts constellation. Ten declaration kinds get their home here, and the
sample provides a live class-template specialization without declaring a
single template: `ShapeList` forces `std::vector<geo::Shape *>` into
existence.

## 8.1 — ClassTemplateDecl

```cpp
CTD->getTemplatedDecl();                    // pattern CXXRecordDecl
CTD->getTemplateParameters();
for (ClassTemplateSpecializationDecl *Spec : CTD->specializations()) { … }
```

Everything from Part 6 applies to the pattern — it's a full
`CXXRecordDecl`, just a *dependent* one (`isDependentContext()` true,
members typed in terms of `T`). The upgrade over function templates:
instantiations get their own node kind rather than reusing the base.

## 8.2 — ClassTemplateSpecializationDecl & TemplateArgument

**`ClassTemplateSpecializationDecl`** is a `CXXRecordDecl` subclass — all
of Part 6 applies — plus the template bookkeeping:

```cpp
Spec->getSpecializedTemplate();             // the ClassTemplateDecl
const TemplateArgumentList &Args = Spec->getTemplateArgs();
Args[0].getKind();                          // TemplateArgument::ArgKind
Spec->getSpecializationKind();              // TSK_ImplicitInstantiation / …
Spec->getInstantiatedFrom();                // primary? partial? (see §8.3)
```

Watch clang-query find the sample's live one:

```
clang-query> match classTemplateSpecializationDecl(hasName("vector"),
             hasTemplateArgument(0, refersToType(pointsTo(cxxRecordDecl(hasName("Shape"))))))
1 match.
```

`TemplateArgument` is its own little taxonomy, worth one table because
`getKind()` gates every accessor:

| ArgKind | What it holds | Accessor |
|---------|---------------|----------|
| `Type` | `vector<Shape*>`'s `Shape*` | `getAsType()` |
| `Integral` | `array<int, 3>`'s `3` | `getAsIntegral()` |
| `Declaration` | pointer/reference NTTP naming an entity | `getAsDecl()` |
| `NullPtr` | `nullptr` NTTP | — |
| `Template` | argument for a template-template param | `getAsTemplate()` |
| `Expression` | not-yet-evaluated (dependent contexts) | `getAsExpr()` |
| `Pack` | expansion of a parameter pack | `pack_elements()` |
| `StructuralValue` | C++20 class-type/float NTTPs | `getAsStructuralValue()` |
| `TemplateExpansion`, `Null` | pack-of-templates / empty marker | — |

## 8.3 — ClassTemplatePartialSpecializationDecl

`template <class T> struct Traits<T *> { … };` — a
**`ClassTemplatePartialSpecializationDecl`**: a *pattern with constraints
on the argument shape*, never instantiated directly:

```cpp
Partial->getTemplateParameters();       // its own params (the T in <T *>)
Partial->getTemplateArgsAsWritten();    // the <T *> pattern being matched
```

Concrete uses still produce plain `ClassTemplateSpecializationDecl`s;
`Spec->getInstantiatedFrom()` returns a pointer-union telling you whether
the primary or a particular partial won the match — the AST records the
outcome of partial ordering, so tools never re-derive it.

## 8.4 — VarTemplateDecl & its specializations

`template <class T> constexpr T pi = T(3.14159);` mirrors the class-side
trio exactly — verified dump, abridged:

```
VarTemplateDecl … pi
├─VarDecl … pi 'const T' cinit                                ← pattern
├─VarTemplateSpecializationDecl … pi 'const double' implicit_instantiation
└─VarTemplateSpecializationDecl … pi 'int *const' implicit_instantiation
VarTemplatePartialSpecializationDecl … pi 'T *const' explicit_specialization
```

**`VarTemplateDecl`** wraps a `VarDecl` pattern;
**`VarTemplateSpecializationDecl`** (a `VarDecl` subclass) is one
instantiation per argument list, and
**`VarTemplatePartialSpecializationDecl`** handles `pi<T *>`-style
patterns. The API surface is the class-template one with `Var` substituted
— `specializations()`, `getTemplateArgs()`, `getSpecializationKind()`.
Every `DeclRefExpr` to `pi<double>` points at the specialization node, so
"find uses of this variable template" reduces to Part 16's reference
chasing.

## 8.5 — TypeAliasTemplateDecl

`template <class T> using Vec = std::vector<T>;` — **`TypeAliasTemplateDecl`**
wraps a `TypeAliasDecl` pattern (Part 9 §9.3). The interesting asymmetry:
alias templates are *transparent*. Using `Vec<int>` never creates a "Vec
instantiation" decl; it resolves straight to `vector<int>` in the type
system, leaving only sugar behind (Part 28 shows the
`TemplateSpecializationType` it leaves in its wake). So
`specializations()` doesn't exist here — there is nothing to enumerate.
Tools that want "all uses of the alias template" must search *types*, not
declarations: one of the clearest examples of the AST-is-a-graph point from
Part 2.

**Quiz.** Your tool visits a `CXXRecordDecl` named `vector` with methods,
fields, bases — everything Part 6 promised. How do you tell whether you're
inside the *pattern* (`T`s everywhere) or inside a concrete instantiation,
without looking at any types?

> [!hint]- Hint
> One of the two is a more-derived node class; `dyn_cast` is enough. For
> the general query there's also a method on `CXXRecordDecl` about
> "described" templates.

> [!success]- Answer
> `dyn_cast<ClassTemplateSpecializationDecl>(RD)` succeeds only for
> instantiations/explicit specializations. The pattern answers via
> `RD->getDescribedClassTemplate() != nullptr` (it's the `CXXRecordDecl`
> wrapped by the `ClassTemplateDecl`). Belt-and-braces:
> `RD->isDependentContext()` is true inside the pattern.

## 8.6 — Concepts: ConceptDecl, ImplicitConceptSpecializationDecl, RequiresExprBodyDecl

The C++20 constellation, verified against this snippet (`-std=c++20`):

```cpp
template <class T> concept Small = sizeof(T) <= 8 && requires(T a) { a + a; };
template <Small T> T twice(T v) { return v + v; }
int r = twice(21);
```

```
ConceptDecl … Small
└─BinaryOperator … '&&'
  ├─BinaryOperator … '<='
  └─RequiresExpr … 'bool'
…
ConceptSpecializationExpr … 'bool' Concept 'Small'
└─ImplicitConceptSpecializationDecl …
```

The three declaration kinds:

- **`ConceptDecl`** — the concept itself: a `TemplateDecl` whose "pattern"
  is the constraint expression (`getConstraintExpr()`).
- **`ImplicitConceptSpecializationDecl`** — the compiler's record that
  `Small<int>` was checked for specific arguments; it hangs under each
  `ConceptSpecializationExpr` (the *expression* side lives in Part 23).
- **`RequiresExprBodyDecl`** — the invisible `DeclContext` inside a
  `requires (T a) { … }` body, holding the local parameter `a`. It rarely
  prints its own dump line, but it's the `getDeclContext()` of everything
  declared inside the requires-expression — visitors encounter it as a
  context, not as content.

Constrained declarations round out the API: functions answer
`getTrailingRequiresClause()`, template parameter lists carry
`getRequiresClause()`, and constrained `TemplateTypeParmDecl`s report
`hasTypeConstraint()` (Part 7 §7.2).

For tooling purposes the important behavior: an *unsatisfied* constraint
doesn't produce broken instantiations in the tree — the candidate simply
drops out of overload resolution, so the AST you get is the
post-resolution truth, same as Part 5's overloads.
