# Part 30 — Visitors

[← Part 29 — TypeLoc](part_29_types_typeloc.md) | [Part 31 — SourceManager →](part_31_source_manager.md)

A visitor is the imperative way to consume the AST: Clang walks the whole
tree, and calls you back on the node kinds you declared interest in. It's the
right tool when you need full control, cross-node state, or a whole-program
inventory — and it's what the visitor-tool skeleton has been waiting for.

## 30.1 — Where a visitor plugs in

Recall the skeleton's chain: `ClangTool → Action → Consumer →
HandleTranslationUnit(ASTContext&)`. The consumer is handed a *finished* AST;
what it does next is entirely up to you. The skeleton does the canonical
thing:

```cpp
void HandleTranslationUnit(ASTContext &Ctx) override {
  Visitor V(Ctx);
  V.TraverseDecl(Ctx.getTranslationUnitDecl());   // walk everything from the root
}
```

Nothing stops you from traversing only part of the tree — pass any `Decl*` or
`Stmt*` to the matching `TraverseXxx` and the walk starts there. That's a
useful trick for "analyze just this one function" tools.

## 30.2 — RecursiveASTVisitor: Visit / Traverse / WalkUpFrom

`RecursiveASTVisitor` is a CRTP template — you inherit passing *yourself* as
the template argument:

```cpp
class Visitor : public RecursiveASTVisitor<Visitor> { … };
```

There's no virtual dispatch: the base template statically detects which
methods you defined. You can override at three levels, from common to rare:

- **`bool VisitFunctionDecl(FunctionDecl *FD)`** — "call me for every node of
  this kind." 95% of visitors only ever write `Visit` methods. Works for any
  node class, at any level of the hierarchy — `VisitStmt` sees every
  statement, `VisitNamedDecl` every named declaration.
- **`bool TraverseIfStmt(IfStmt *S)`** — "I'll handle recursing into this
  subtree myself." Override to *skip* a subtree, reorder it, or push/pop
  state around it (call the base method in the middle to keep recursion).
- **`bool WalkUpFromBinaryOperator(BinaryOperator *BO)`** — controls the
  is-a chain: walk-up is what calls `VisitStmt`, then `VisitExpr`, then
  `VisitBinaryOperator` for one node. You almost never touch this.

Return `true` to keep going; return **`false` to abort the entire
traversal** — that's the early-exit mechanism (e.g. "found what I was
looking for").

The traversal is preorder (parents before children) by default.

## 30.3 — Hands-on: a declaration inventory

Grow the skeleton's `Visitor` into a small inventory tool:

```cpp
class Visitor : public RecursiveASTVisitor<Visitor> {
public:
  explicit Visitor(ASTContext &Ctx) : Ctx(Ctx) {}

  bool VisitFunctionDecl(FunctionDecl *FD) {
    if (!Ctx.getSourceManager().isInMainFile(FD->getLocation()))
      return true;                        // skip the #include noise
    llvm::outs() << (FD->isThisDeclarationADefinition() ? "def  " : "decl ")
                 << FD->getQualifiedNameAsString()
                 << " (" << FD->getNumParams() << " params)\n";
    return true;
  }

  bool VisitCXXRecordDecl(CXXRecordDecl *RD) {
    if (!Ctx.getSourceManager().isInMainFile(RD->getLocation()) ||
        !RD->isThisDeclarationADefinition())
      return true;
    llvm::outs() << "class " << RD->getQualifiedNameAsString();
    for (const CXXBaseSpecifier &B : RD->bases())
      llvm::outs() << " : " << B.getType().getAsString();
    llvm::outs() << "\n";
    return true;
  }

private:
  ASTContext &Ctx;
};
```

```bash
cmake --build build
./build/visitor-tool sample/sample.cpp -- -std=c++17
```

You should see `geo::Point`, `geo::Shape`, `geo::Circle : geo::Shape`, the
free functions, `main` — plus a few things you may not expect (next section
explains them).

Two details in that code carry most of the lessons of Parts 3 and 31:
`isInMainFile` keeps the tool from drowning in `<vector>` internals, and
`isThisDeclarationADefinition` deduplicates forward declarations from
definitions — a `Decl` node exists for *each* declaration of an entity, and
they're linked as a redeclaration chain (`FD->getDefinition()` hops to the
one with the body).

**Quiz.** The output lists functions you never wrote — `Point::Point`,
`Circle::operator=`, and the lambda's `operator()`. Where do they come from?

> [!success]- Answer
> Implicit members: the compiler-generated default/copy/move constructors and
> assignment operators exist as real `CXXMethodDecl` nodes (marked `implicit`
> in a dump). The lambda's `operator()` is the closure type's call operator —
> written by you in effect, but housed in an unnamed implicit class. Filter
> with `FD->isImplicit()` if you only want spelled-out code.

## 30.4 — Controlling the traversal

`RecursiveASTVisitor` has opt-in knobs, expressed as methods you define:

```cpp
bool shouldVisitImplicitCode() const { return true; }
bool shouldVisitTemplateInstantiations() const { return true; }
```

By default the visitor **skips** implicit code bodies (e.g. the contents of
compiler-generated constructors) and **skips** template instantiations —
`geo::clamp` is visited once as its dependent pattern, not once per
`clamp<int>`, `clamp<double>`. Flip the knob and each instantiation's
`FunctionDecl` (with concrete types!) is visited too. Which one you want
depends entirely on the question: "is this template well-styled?" → pattern;
"can this arithmetic overflow for the types actually used?" → instantiations.

To skip a subtree, override its `Traverse` method:

```cpp
bool TraverseCXXRecordDecl(CXXRecordDecl *RD) {
  if (RD->isLambda())
    return true;          // don't descend into lambda closure types at all
  return RecursiveASTVisitor::TraverseCXXRecordDecl(RD);
}
```

## 30.5 — Getting context: parents and DeclContext

A visitor callback gets a node in isolation — but real checks are contextual:
*"a `return` inside a destructor"*, *"a call inside a loop"*. Two mechanisms
give you the surroundings.

**Semantic nesting: `DeclContext`.** Every `Decl` knows the declaration it
lives in, and contexts chain to the root:

```cpp
bool VisitFunctionDecl(FunctionDecl *FD) {
  for (const DeclContext *DC = FD->getDeclContext(); DC; DC = DC->getParent())
    if (const auto *NS = dyn_cast<NamespaceDecl>(DC))
      llvm::outs() << FD->getNameAsString() << " is inside namespace "
                   << NS->getNameAsString() << "\n";
  return true;
}
```

This answers "what namespace/class/function *declares* this?" It's cheap and
always available — but it only exists for declarations.

**Tree structure: `ASTContext::getParents`.** For statements and expressions
("what statement contains this call?"), ask the context for the node's tree
parents:

```cpp
#include "clang/AST/ParentMapContext.h"

bool VisitCallExpr(CallExpr *CE) {
  DynTypedNodeList Parents = Ctx.getParents(*CE);
  if (!Parents.empty())
    if (const auto *FD = Parents[0].get<FunctionDecl>())
      llvm::outs() << "call directly inside function "
                   << FD->getNameAsString() << "\n";
  return true;
}
```

The result is a list of `DynTypedNode` — the type-erased glue from Part 2's
“no common base” problem: it can hold a node from *any* hierarchy, and
`get<T>()` gives you a typed pointer back (null if the parent isn't a `T`).
It's a list because a handful of nodes (template instantiations) genuinely
have multiple parents; for ordinary code there's exactly one. To find the
*enclosing function* rather than the immediate parent, loop: take
`Parents[0]`, ask for *its* parents, until `get<FunctionDecl>()` hits.

The first `getParents` call builds a parent map for the whole TU (one full
traversal — noticeable on big files, fine everywhere else).

**Quiz.** Why doesn't every AST node simply store a parent pointer, the way
DOM nodes do?

> [!success]- Answer
> Nodes are built bottom-up during parsing (children exist before parents) and
> are meant to be small and immutable; types are even *shared* between users,
> so a single parent is ill-defined. Parents are derivable, so Clang computes
> the map lazily only for tools that ask.

## 30.6 — DynamicRecursiveASTVisitor

The CRTP visitor's price is compile time and code size: every visitor class
instantiates a ~600-method template. Since LLVM 20 there's a virtual-dispatch
alternative with the same shape:

```cpp
#include "clang/AST/DynamicRecursiveASTVisitor.h"

class Inventory : public DynamicRecursiveASTVisitor {
public:
  bool VisitFunctionDecl(FunctionDecl *FD) override {   // note: override!
    …
    return true;
  }
};
```

Same method names, same semantics — but ordinary virtual functions, so you
get `override` checking (the classic CRTP failure is silently never being
called because of a typo like `VisitFunctionDec`), much faster builds, and
the traversal-control knobs become plain bools you set
(`ShouldVisitImplicitCode = true;`). Clang's own tree is migrating to it;
prefer it for new tools unless you need the last few percent of runtime speed
or `Traverse`-level customization of the hot path.
