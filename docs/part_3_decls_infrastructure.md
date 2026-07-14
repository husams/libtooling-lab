# Part 3 — Declarations I: The Decl Infrastructure

[← Part 2](part_2_ast_fundamentals.md) | [Part 4 — Variables & Fields →](part_4_decls_variables.md)

Declarations are where AST work usually starts: they carry the names, the
types, and the structure of a program, and every other node hangs off one
sooner or later. Parts 3–10 cover **all 91 concrete declaration kinds** this
LLVM defines. This first part is the machinery every `Decl` shares — names,
contexts, redeclaration chains — plus the map of how the family is organized,
so the seven parts that follow always have a place to hang a new kind.

## 3.1 — The Decl base class

Every declaration node derives from `Decl` — 91 concrete kinds in LLVM 22,
from `TranslationUnitDecl` down to `EmptyDecl` (a stray `;` at namespace
scope). What they all share:

```cpp
D->getKind();                 // Decl::Kind enum value
D->getDeclKindName();         // "Function", "CXXRecord", "Var", …
D->getLocation();             // the declaration's "interesting" point (usually the name)
D->getBeginLoc(); D->getEndLoc();   // full extent, e.g. specifiers through '}' or ';'
D->isImplicit();              // compiler-generated, no source text
D->isInvalidDecl();           // parsed with errors — treat contents as suspect
D->getASTContext();           // back-pointer to the owning context
D->getAccess();               // AS_public/…/AS_none — meaningful inside classes
D->attrs();                   // attached attributes (Part 10)
```

Two of these deserve a habit. `isImplicit()` is the filter that separates
what the programmer wrote from what the compiler manufactured (Part 6 shows
just how much of a class is manufactured). And `isInvalidDecl()` matters the
moment your tool runs on code that doesn't compile cleanly — Clang recovers
from errors and hands you a best-effort node, flagged invalid; analysis that
ignores the flag draws conclusions from wreckage.

## 3.2 — Names: DeclarationName

`NamedDecl` adds a name — but a C++ "name" is not always an identifier. The
name of a constructor is a *type*; the name of `operator+` is an operator
symbol; the name of `operator double()` is another type. Clang models this
with `DeclarationName`, a discriminated union:

| Name kind | Example declaration |
|-----------|---------------------|
| `Identifier` | `totalArea`, `radius_`, `Circle` (the class) |
| `CXXConstructorName` | `Circle::Circle` |
| `CXXDestructorName` | `Shape::~Shape` |
| `CXXOperatorName` | `operator=`, `operator+` |
| `CXXConversionFunctionName` | `operator double()` |
| `CXXLiteralOperatorName` | `operator""_km` |
| `CXXDeductionGuideName` | deduction guides (Part 5 §5.6) |
| `CXXUsingDirective` | internal marker for `using namespace` |
| `ObjC*Selector` (three kinds) | Objective-C only — you won't meet them here |

This is why the accessor you reach for matters:

```cpp
ND->getDeclName();               // the DeclarationName itself
ND->getDeclName().isIdentifier();// safe to ask
ND->getName();                   // StringRef — ASSERTS unless it's an identifier
ND->getNameAsString();           // works for every kind: "Circle", "operator=", "~Shape"
ND->getQualifiedNameAsString();  // adds the path: "geo::Circle::area"
```

The trap is real and was demonstrated in this lab's own sample: calling
`getName()` while visiting every `FunctionDecl` aborts the process the
moment it reaches `Circle::Circle` —

```
Assertion failed: (Name.isIdentifier() && "Name is not a simple identifier")
```

Default to `getNameAsString()`; use `getName()` only inside a
`getDeclName().isIdentifier()` check (it's cheaper — no `std::string`
allocation — which is why hot paths in Clang itself use it).

## 3.3 — DeclContext: where declarations live

`DeclContext` is the second base class of every declaration that can
*contain* other declarations: `TranslationUnitDecl`, `NamespaceDecl`,
`TagDecl` (classes/enums), `FunctionDecl` (its parameters and local types
live in it), `LinkageSpecDecl`, and friends. It gives the AST its
scope structure:

```cpp
const DeclContext *DC = FD->getDeclContext();     // who declares me?
for (; DC; DC = DC->getParent())                  // walk outward to the TU
  llvm::outs() << cast<Decl>(DC)->getDeclKindName() << "\n";

for (const Decl *D : RD->decls()) { … }           // iterate members in order
auto Results = DC->lookup(&Ctx.Idents.get("area"));  // scope-aware name lookup
```

Note the double nature: a `CXXRecordDecl` *is-a* `Decl` and *is-a*
`DeclContext` at once — `cast<Decl>(DC)` and `dyn_cast<DeclContext>(D)`
convert between the views.

There are two parent notions, and template/out-of-line code makes them
differ. `getDeclContext()` is the **semantic** parent — where the entity
logically belongs. `getLexicalDeclContext()` is the **lexical** parent —
where the text appears. For a method defined out of line
(`double Circle::area() const { … }` at namespace scope), the semantic
context is `Circle`, the lexical context is namespace `geo`. Tools that
group output "by class" want the former; tools that care about file layout
want the latter.

## 3.4 — Redeclaration chains

C++ lets an entity be declared many times and defined once — and Clang
creates a **separate node per declaration**, linked in a chain. This is the
most common source of double-counting bugs in tools:

```cpp
FD->isThisDeclarationADefinition();  // is THIS node the one with the body?
FD->getDefinition();                 // hop to the defining node (or null)
FD->getCanonicalDecl();              // the chain's canonical representative
for (const FunctionDecl *R : FD->redecls()) { … }   // walk the whole chain
```

The rules of thumb: **deduplicate with `getCanonicalDecl()`** (two nodes
refer to the same entity iff their canonical decls are pointer-equal), and
**analyze bodies via `getDefinition()`**. A visitor that counts
`FunctionDecl`s in a header-heavy TU without canonicalizing will happily
report the same function five times.

**Quiz.** A header declares `void f();` and a `.cpp` file defines it. Your
tool visits the TU for that `.cpp` and sees two `FunctionDecl`s for `f`.
Which of the following are pointer-equal: their `getCanonicalDecl()`, their
`getDefinition()`, the nodes themselves?

> [!hint]- Hint
> Each declaration is its own node; the chain accessors exist precisely to
> relate them.

> [!success]- Answer
> The nodes themselves differ. Both `getCanonicalDecl()` calls return the
> *first* declaration (the header's), and both `getDefinition()` calls
> return the `.cpp` node — so both accessors are pointer-equal across the
> two, and either works as a dedup key. (Convention: canonical for
> identity, definition for bodies.)

## 3.5 — The complete kind map: DeclNodes.inc

The authoritative list of declaration kinds is generated into your own
LLVM installation — the same file this lab's coverage was extracted from:

```bash
less $LLVM/include/clang/AST/DeclNodes.inc
```

Three things to know about reading it. Concrete kinds appear as
`SOMENAME(Name, Base)` entries. **Abstract** intermediate classes
(`NamedDecl`, `ValueDecl`, `DeclaratorDecl`, `TypeDecl`, `TagDecl`,
`TemplateDecl`, `RedeclarableTemplateDecl`…) are wrapped in
`ABSTRACT_DECL(…)` — they never appear in a dump, but `dyn_cast` to them
works and is how you write "any declaration with a type" checks. And
`DECL_RANGE` entries define the contiguous kind ranges that make
`isa<TagDecl>(D)` a constant-time comparison instead of a chain of tests.

The 91 concrete kinds, mapped to where this lab covers them:

| Cluster | Kinds | Part |
|---------|-------|------|
| Variables, parameters, fields, bindings | 8 | Part 4 |
| Functions, methods, special members, deduction guides | 6 | Part 5 |
| Records, friends, access specifiers | 5 | Part 6 |
| Template parameters & function templates | 5 | Part 7 |
| Class/variable/alias templates, concepts | 10 | Part 8 |
| Enums, typedefs/aliases, namespaces, `using` family | 15 | Part 9 |
| Everything else, incl. ObjC/OpenMP/OpenACC/HLSL tables | 42 | Part 10 |

Every kind name in these parts is written in backticks exactly as it
appears in `DeclNodes.inc` (plus the `Decl` suffix), so the docs are
greppable the same way the AST is — and `node_index.md` is the one-page
lookup from any kind to its lesson.

**Quiz.** `isa<DeclaratorDecl>(D)` — which of these return true:
a `ParmVarDecl`, a `FieldDecl`, an `EnumConstantDecl`, a `CXXMethodDecl`?

> [!hint]- Hint
> `DeclaratorDecl` means "declared with declarator syntax — a type written
> around a name". Enumerators are just names with values.

> [!success]- Answer
> All except `EnumConstantDecl`. Parameters, fields, and methods are all
> written as type-around-name declarators (and so carry type-source
> information, Part 29); an enumerator is a bare name — it's a `ValueDecl`
> but not a `DeclaratorDecl`.
