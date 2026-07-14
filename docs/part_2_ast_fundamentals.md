# Part 2 — AST Fundamentals

[← Part 1](part_1_setup_first_tool.md) | [Part 3 — Declarations: Infrastructure →](part_3_decls_infrastructure.md)

Everything in this lab — visitors, matchers, rewriting — is a way of asking
questions about Clang's AST, and Parts 2 through 29 are a systematic tour of
it: this part builds the mental model and the exploration workflow, then the
deep dive covers every node kind Clang defines — declarations in Parts 3–10,
statements in 11–13, expressions in 14–24, and the type system in 25–29.
Fluency here is the highest-leverage skill in Clang tooling.

## 2.1 — Seeing the AST: -ast-dump and friends

Clang will print its AST for any file. Run this against the lab sample:

```bash
cd skeletons/visitor-tool
$LLVM/bin/clang++ -std=c++17 -Xclang -ast-dump -fsyntax-only sample/sample.cpp | less -R
```

That's overwhelming — the `#include`s drag in thousands of declarations.
Filter to one declaration at a time instead; this is the command you'll use
hundreds of times across the next twenty-seven parts:

```bash
$LLVM/bin/clang++ -std=c++17 -Xclang -ast-dump -Xclang -ast-dump-filter -Xclang totalArea \
  -fsyntax-only sample/sample.cpp
```

```
FunctionDecl … totalArea 'double (const ShapeList &)' static
├─ParmVarDecl … shapes 'const ShapeList &'
└─CompoundStmt
  ├─DeclStmt
  │ └─VarDecl … total 'double' cinit
  │   └─FloatingLiteral … 'double' 0.000000e+00
  ├─CXXForRangeStmt
  │ …
  └─ReturnStmt
    └─ImplicitCastExpr … 'double' <LValueToRValue>
      └─DeclRefExpr … 'double' lvalue Var … 'total' 'double'
```

Read one line of a dump as: **node kind**, memory address, source range in
`<>`, then the **type** in quotes, then node-specific details. The variants
worth knowing: `-ast-dump=json` (machine-readable), `-ast-dump-lookups`
(name-lookup tables), and — once you reach Part 33 — `clang-query`'s
`set output dump`, which is this same dump scoped to whatever a matcher
finds.

## 2.2 — ASTContext and TranslationUnitDecl

Two objects own everything you just saw:

- **`ASTContext`** — owns the memory of all nodes, the identifier and type
  tables, the `SourceManager`, the language options. Any question that isn't
  about one node in isolation ("what's the size of this type?", "who is this
  node's parent?") goes through the context. Your `ASTConsumer` receives it
  in `HandleTranslationUnit(ASTContext &)`.
- **`TranslationUnitDecl`** — the root of the tree,
  `Ctx.getTranslationUnitDecl()`. One per parsed file (a *translation unit* =
  the file plus everything it includes).

The split matters practically: nodes are small and dumb on purpose. A
`SourceLocation` is meaningless without the context's `SourceManager`; a
type's size is unknowable without the context's target info. Tool code
therefore almost always carries an `ASTContext&` alongside whatever node it's
looking at — which is why the skeleton's `Visitor` stores one.

## 2.3 — Three hierarchies, no common base

The AST is not one class hierarchy. It's three separate ones, plus glue:

```
Decl   — things that declare: functions, variables, classes, namespaces…   (Parts 3-10)
Stmt   — things that happen: blocks, ifs, loops, returns…                  (Parts 11-13)
         └─ Expr — statements that produce a value (yes, Expr is-a Stmt)   (Parts 14-24)
Type   — what things are: int, Shape*, std::string…  (immutable, uniqued)  (Parts 25-29)
```

There is **no common `ASTNode` base class**. A `Decl` is not a `Stmt`; you
can't put them in one container or write one function over both without help.
Each hierarchy has its own kind discrimination and its own casting support.

The hierarchies reference each other constantly: a `FunctionDecl` (Decl) has
a body (`Stmt`); a `DeclStmt` (Stmt) wraps a `VarDecl` (Decl); a `VarDecl`
has a `QualType` (Type); and a `DeclRefExpr` (Expr) points back at the
`ValueDecl` it names. That last edge deserves emphasis: **the AST is a
graph, not just a tree**. Reference edges cross the tree structure, and
they're how you answer "what does this name refer to?" without doing name
lookup yourself — the compiler already did it.

**Quiz.** Why did Clang's designers *not* give `Decl`, `Stmt`, and `Type` a
common base class, when almost every other AST library has one?

> [!hint]- Hint
> Think about what `Type` nodes are (uniqued, shared, immutable) versus what
> `Stmt` nodes are (one per occurrence in source), and what a universal base
> would force both to pay for.

> [!success]- Answer
> The families have incompatible economics. A `Type` exists once per
> translation unit no matter how many places use it, so it can't carry a
> source location or a parent; a `Stmt` exists per occurrence and can.
> Locations, allocation strategy, and traversal all differ, so a shared base
> would either bloat every node or be an empty marker. When type-erased
> handling is genuinely needed, `DynTypedNode` (next section) provides it at
> the edges instead of in the class hierarchy.

## 2.4 — Node identity: kinds, casts, DynTypedNode

Within one hierarchy, LLVM uses its own runtime-type machinery (remember
`-fno-rtti` from Part 1 — this replaces C++ RTTI):

```cpp
D->getKind();                       // Decl::Kind enum: Decl::Function, Decl::CXXRecord…
S->getStmtClassName();              // "IfStmt" — handy in debug output

if (const auto *FD = llvm::dyn_cast<FunctionDecl>(D))   // downcast-or-null
  use(FD);
llvm::isa<CXXMethodDecl>(D);                            // test only
llvm::cast<FunctionDecl>(D);                            // assert-on-mismatch
// dyn_cast_or_null / cast_or_null accept a null pointer too
```

`dyn_cast` respects the whole inheritance chain: a `CXXMethodDecl` *is* a
`FunctionDecl`, is a `ValueDecl`, is a `NamedDecl` — so a
`dyn_cast<NamedDecl>` succeeds on all of them. The per-family trees are deep
and worth skimming once: Parts 3–29 map them node kind by node kind.

For the rare code that must hold "some node from any hierarchy" — parent
maps, matcher results, generic utilities — there's **`DynTypedNode`**:

```cpp
DynTypedNode N = DynTypedNode::create(*FD);
N.getNodeKind().asStringRef();       // "FunctionDecl"
const auto *AsFn = N.get<FunctionDecl>();   // typed pointer back, or null
N.getSourceRange();                  // works across hierarchies
```

You'll meet it as the return currency of `ASTContext::getParents` (Part 30)
and inside every matcher result (Part 33).

## 2.5 — A workflow for exploring any node

Twenty-seven parts of node kinds lie ahead; nobody memorizes them. What you build
instead is a loop for answering "what *is* this thing and what can I ask
it?" — four tools, cheapest first:

1. **Dump it.** Every node has `->dump()` (or `->dump(llvm::errs(), Ctx)`),
   callable from anywhere in your tool, even mid-debugging session. On the
   command line, `-ast-dump-filter <name>` scopes the noise away.
2. **Interrogate it in clang-query** (Part 33 covers the syntax; it's useful
   long before then): `match functionDecl(hasName("clamp"))` + `set output
   dump` answers most "what does the tree look like here?" questions in
   seconds.
3. **Read the header.** The class comments in `$LLVM/include/clang/AST/`
   (`Decl.h`, `DeclCXX.h`, `Stmt.h`, `Expr.h`, `ExprCXX.h`, `Type.h`) are
   excellent, current by construction, and list every accessor. When docs
   and web posts disagree with the header, the header wins.
4. **Check the class reference** at
   [clang.llvm.org/doxygen](https://clang.llvm.org/doxygen/) for the
   rendered version of the same.

A worked example of the loop — "what is `geo::clamp` in the AST?":

```bash
$LLVM/bin/clang++ -std=c++17 -Xclang -ast-dump -Xclang -ast-dump-filter -Xclang clamp \
  -fsyntax-only sample/sample.cpp | head -30
```

The dump shows a `FunctionTemplateDecl` wrapping a pattern `FunctionDecl` —
*and* a second `FunctionDecl` for `clamp<int>` marked as a specialization.
Two node kinds you may not have expected, one command to discover them, and
Part 7 to explain them. That's the loop; run it on anything that surprises
you from here on.
