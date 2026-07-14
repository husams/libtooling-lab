# Part 7 — Declarations V: Template Parameters & Function Templates

[← Part 6](part_6_decls_records.md) | [Part 8 — Templates II →](part_8_decls_templates_classes.md)

Templates are where the AST stops being a picture of the source and becomes
a picture of the *compilation*: one entity in the code can produce many
nodes in the tree. This part establishes the two-level model on
`geo::clamp`, then covers the parameter declarations
(`TemplateTypeParmDecl`, `NonTypeTemplateParmDecl`,
`TemplateTemplateParmDecl`) and `FunctionTemplateDecl` itself; Part 8
extends the same model to classes, variables, aliases, and concepts.

## 7.1 — The two-level model

Dump the sample's template and read the shape carefully:

```bash
$LLVM/bin/clang++ -std=c++17 -Xclang -ast-dump -Xclang -ast-dump-filter \
  -Xclang geo::clamp -fsyntax-only sample/sample.cpp
```

```
FunctionTemplateDecl … clamp
├─TemplateTypeParmDecl … referenced typename depth 0 index 0 T
├─FunctionDecl … clamp 'T (T, T, T)'                       ← the PATTERN
└─FunctionDecl … used clamp 'int (int, int, int)'          ← an INSTANTIATION
     implicit_instantiation instantiated_from 0x…
  └─TemplateArgument type 'int'
```

Three declarations for one line of code:

- the **`FunctionTemplateDecl`** — the template as an entity: name,
  parameter list, and a link to the pattern;
- the **pattern** `FunctionDecl` — body with *dependent* types (`T (T, T,
  T)`; comparisons in it can't be resolved yet, which Part 23 revisits);
- one **instantiation** `FunctionDecl` per distinct argument list the
  program actually forced — here `clamp<int>` from `geo::clamp(42, 0, 10)`
  in `main`, with fully concrete types and a `TemplateArgument` recording
  `T = int`.

That's the whole model. Every template flavor — function, class, variable,
alias — is a `*TemplateDecl` wrapping a pattern, with instantiations
materialized on demand elsewhere in the tree. It's also why "how many
functions does this file define?" is an ill-posed question until you decide
how to count templates — and why visitors and matchers each have a knob for
which level they show you (Parts 30 and 33).

## 7.2 — TemplateTypeParmDecl

`typename T` / `class T` is itself a declaration — a `TypeDecl`, since `T`
names a type inside the pattern:

```cpp
TTP->getDepth();               // nesting level of the owning template
TTP->getIndex();               // position in its parameter list
TTP->wasDeclaredWithTypename();// typename vs class spelling
TTP->isParameterPack();        // typename... Ts
TTP->hasDefaultArgument();     // class Alloc = allocator<T>
TTP->getDefaultArgument();
TTP->hasTypeConstraint();      // Small T   (C++20 — Part 8 §8.6)
```

Depth and index are the coordinate system of dependence: the `depth 0
index 0` in the dump above is exactly how a `T` deep inside a body refers
back to its parameter with no name lookup — and Part 28 shows the same pair
resurfacing inside `TemplateTypeParmType`.

## 7.3 — NonTypeTemplateParmDecl

The value-parameter flavor (`int N` in `array<T, N>`) is a
`DeclaratorDecl` — it has a type like a variable does:

```cpp
NTTP->getType();               // int, auto (C++17), a class type (C++20)
NTTP->getDepth();  NTTP->getIndex();  NTTP->isParameterPack();
NTTP->hasDefaultArgument();  NTTP->getDefaultArgument();
NTTP->isExpandedParameterPack();
```

Since C++20 the type can be a structural class type — at which point uses
of the parameter's value inside instantiations refer to a
`TemplateParamObjectDecl`, the compiler-manufactured constant object
covered with its siblings in Part 10 §10.3.

## 7.4 — TemplateTemplateParmDecl & BuiltinTemplateDecl

`template <template <class> class Cont> …` declares a parameter that is
itself a template — a `TemplateDecl` whose own `getTemplateParameters()`
describes the shape it demands. Same coordinates (`getDepth()`,
`getIndex()`, `isParameterPack()`), plus `getDefaultArgument()` naming a
template.

Its rare sibling completes the parameter corner:

| Kind | What it is |
|------|------------|
| `BuiltinTemplateDecl` | compiler-magic "templates" with no C++ definition — `__make_integer_seq` (behind `std::make_integer_sequence`), `__type_pack_element`. They appear in dumps of standard-library internals; `getBuiltinTemplateKind()` says which magic. |

All three parameter kinds share `TemplateParameterList` — reachable from
any template via `getTemplateParameters()`, which also carries the C++20
`requires` clause (`getRequiresClause()`).

## 7.5 — FunctionTemplateDecl

Navigation in all directions:

```cpp
FTD->getTemplatedDecl();          // the pattern FunctionDecl
FTD->getTemplateParameters();     // the parameter list (§7.2-7.4)
for (FunctionDecl *Spec : FTD->specializations()) { … }   // instantiations so far

// …and back, from any FunctionDecl you encounter:
FD->isTemplateInstantiation();
FD->getPrimaryTemplate();               // the FunctionTemplateDecl (or null)
FD->getTemplateSpecializationArgs();    // the deduced <int>
FD->getTemplateInstantiationPattern();  // the pattern this body was cloned from
FD->getTemplatedKind();                 // ordinary / pattern / specialization / …
FD->getDescribedFunctionTemplate();     // pattern → its FunctionTemplateDecl
```

`getTemplatedKind()` is the disambiguator when a visitor hands you an
arbitrary `FunctionDecl`: `TK_NonTemplate` (ordinary function),
`TK_FunctionTemplate` (you're looking at the pattern),
`TK_FunctionTemplateSpecialization` (an instantiation — or an *explicit*
specialization, distinguished by `getTemplateSpecializationKind()`:
`TSK_ImplicitInstantiation` vs `TSK_ExplicitSpecialization`, plus the
explicit-instantiation pair for `template void f<int>();`).

Method templates, constructor templates, conversion-function templates —
none are new kinds. Each is a `FunctionTemplateDecl` living inside a class,
wrapping the appropriate Part 5 node as its pattern. The wrapper composes;
the callable six stay six.

**Quiz.** Your visitor's `VisitFunctionDecl` fires for `clamp` three times
in a TU that calls `clamp(1,2,3)` and `clamp(1.0,2.0,3.0)`. Which node is
which, and which single call distinguishes all three without touching
types?

> [!hint]- Hint
> One is the pattern, two are instantiations; §7.5's disambiguator does it
> in one call.

> [!success]- Answer
> `FD->getTemplatedKind()`: the pattern returns `TK_FunctionTemplate`
> (it's the `FunctionDecl` described by the template), and both
> instantiations return `TK_FunctionTemplateSpecialization` — their
> `getTemplateSpecializationArgs()` then reveal `<int>` vs `<double>`.
> (Visitors skip the instantiations by default;
> `shouldVisitTemplateInstantiations()` — Part 30 — is why you'd see all
> three.)
