# Part 29 — Types V: TypeLoc

[← Part 28 — Deduction & Dependence](part_28_types_deduction_dependence.md) | [Part 30 — Visitors →](part_30_visitors.md)

`Type` nodes are uniqued and shared (Part 25), so they cannot carry source
locations — the one `int` node serves the whole program. Yet rewriting
tools must answer "where exactly was this type *written*?". The answer
lives in a parallel structure: `TypeSourceInfo`, whose payload is a
`TypeLoc`. This short part closes the type world by connecting it back to
the `SourceLocation` machinery you'll sharpen in Part 31.

## 29.1 — TypeSourceInfo and TypeLoc values

Declarations that spell a type carry a `TypeSourceInfo`:

```cpp
if (TypeSourceInfo *TSI = VD->getTypeSourceInfo()) {
  TypeLoc TL = TSI->getTypeLoc();
  TL.getType();                 // the QualType it describes
  TL.getSourceRange();          // exactly where the type was spelled
  TL.getBeginLoc();
}
```

A `TypeLoc` is a *value* — a thin cursor over the `TypeSourceInfo`'s
location data, sized like a couple of pointers. Copy it freely; there's no
node allocation behind it. That's also why the API differs in flavor from
the node hierarchies: you don't `dyn_cast` pointers, you convert cursors.

The null check is not decoration. Every variable *has* a type
(`getType()` never fails), but implicit declarations — compiler-generated
members, builtins, some instantiation artifacts — were never written
anywhere, so there is nothing to describe: no `TypeSourceInfo`. Same spirit
as Part 31's invalid `SourceLocation`s — synthesized entities have
semantics but no source.

## 29.2 — The mirror hierarchy

`TypeLoc` classes mirror the type classes one-for-one: `PointerTypeLoc`
for `PointerType`, `FunctionProtoTypeLoc` for `FunctionProtoType`,
`TypedefTypeLoc`, `TemplateSpecializationTypeLoc`, … Each adds the
location-flavored accessors its type node can't have:

```cpp
if (auto PTL = TL.getAs<PointerTypeLoc>()) {
  PTL.getStarLoc();               // the '*' itself
  TypeLoc Inner = PTL.getPointeeLoc();   // cursor over what it points at
}

if (auto FTL = TL.getAs<FunctionProtoTypeLoc>()) {
  FTL.getLParenLoc();  FTL.getRParenLoc();
  FTL.getParam(0);                // the ParmVarDecl, position-aware
  FTL.getReturnLoc();
}

if (auto TSTL = TL.getAs<TemplateSpecializationTypeLoc>()) {
  TSTL.getTemplateNameLoc();      // 'vector' itself, without arguments
  TSTL.getLAngleLoc();  TSTL.getArgLoc(0);
}
```

Unwrapping uses `getAs<XxxTypeLoc>()` — returns an *invalid* (falsy)
`TypeLoc` on mismatch rather than null — plus two structural moves:
`TL.getNextTypeLoc()` steps one layer inward (the TypeLoc equivalent of
`desugar()`), and `TL.getUnqualifiedLoc()` steps past qualifiers, and
`TL.IgnoreParens()` past parentheses.

## 29.3 — The recipes

The situations that force you down to `TypeLoc`, with their one-liners:

**Rename a type as written.** A rewriting tool replacing `ShapeList` with
`Shapes` must edit each *spelling*, never the canonical type (which has no
location) — and must skip spellings inside macros (Part 31):

```cpp
bool VisitTypedefTypeLoc(TypedefTypeLoc TL) {   // visitor: Part 30
  if (TL.getTypedefNameDecl()->getNameAsString() == "ShapeList")
    Edits.push_back(TL.getNameLoc());           // exactly the token to replace
  return true;
}
```

**Find where `auto` was spelled.** The deduced `AutoType` (Part 28) knows
what it became but not where it sits; the `AutoTypeLoc` does:

```cpp
if (auto ATL = TL.getAs<AutoTypeLoc>())
  llvm::outs() << "auto at " << ATL.getNameLoc().printToString(SM) << "\n";
```

**Insert before/after a type.** "Add `const` in front of this parameter's
type" is `TL.getBeginLoc()` + an insertion;
"append `*`" is `Lexer::getLocForEndOfToken(TL.getEndLoc(), …)` — the
Part 32 helper, because `getEndLoc()` points at the *start* of the last
token, the same token-range convention every AST range follows.

**Quiz.** Your rename-`ShapeList` tool works on the sample but misses the
occurrence in `SQUARE`-style macro-generated code, and double-edits one
spelled inside a template instantiated twice. Which two guards fix it?

> [!hint]- Hint
> One guard is about locations (Part 31), one about the two-level template
> model (Parts 7/28).

> [!success]- Answer
> Skip macro locations — `if (TL.getNameLoc().isMacroID()) return true;` —
> and deduplicate instantiations by visiting only the pattern (don't enable
> `shouldVisitTemplateInstantiations`; the spelling exists once in the
> pattern, which is where the edit belongs). Rewrites keyed on
> `(file, offset)` pairs also self-deduplicate — the belt to this
> suspenders.

## 29.4 — TypeLocs in visitors and matchers

Both traversal systems treat `TypeLoc`s as first-class:

- `RecursiveASTVisitor` (Part 30) offers `VisitTypeLoc(TypeLoc)` and
  per-class hooks (`VisitPointerTypeLoc`, `VisitAutoTypeLoc`, …) — the
  rename recipe above is a complete visitor.
- Matchers (Part 33) have a parallel entry: `typeLoc(loc(qualType(...)))`
  binds locations, and `hasTypeLoc(...)` climbs from declarations into
  their spelled types.

The standing rule of thumb that summarizes five parts of type machinery:
**read types through `QualType`, compare them through canonical types,
and touch source only through `TypeLoc`.** With that, the type system —
all 59 node kinds of it — is fully mapped, and everything that remains in
the lab is machinery for *finding* nodes (Parts 30–33) and shipping tools
(Parts 34–35).
