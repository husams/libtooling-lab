# Part 38 — Rewriting & Edits: Rewriter, Replacements & the Edit Library

[← Part 37 — The Index Library](part_37_index.md) | [Part 39 — Analysis: the CFG →](part_39_analysis_cfg.md)

Every part so far *read* the AST. This one writes back. Clang never mutates
source through the AST — the tree is immutable and its nodes carry no text,
only `SourceLocation`s. Rewriting is therefore a separate layer that works on
the *buffers* the SourceManager holds: you express edits as
(location, text) operations, and a rewrite buffer replays them to produce new
source. There are three tiers of this machinery, from the raw `Rewriter` up
to serializable `Replacements`; this part walks all three.

The fifth skeleton, `skeletons/rewrite-tool/`, is wired for it — same build
dance:

```bash
cd skeletons/rewrite-tool
cmake -G Ninja -B build -DCMAKE_PREFIX_PATH=$(brew --prefix llvm) \
  -DCMAKE_CXX_COMPILER=$(brew --prefix llvm)/bin/clang++ .
cmake --build build
./build/rewrite-tool sample/sample.cpp -- -std=c++17
```

## 38.1 — The Rewriter

`Rewriter` (`clang/Rewrite/Core/Rewriter.h`) is the workhorse. Construct it
over the SourceManager and LangOptions — both live on the `ASTContext` — and
issue edits keyed by `SourceLocation`:

```cpp
#include "clang/Rewrite/Core/Rewriter.h"

Rewriter R(Ctx.getSourceManager(), Ctx.getLangOpts());
R.InsertText(Loc, "override ", /*InsertAfter=*/true);   // add text at a point
R.ReplaceText(SourceRange(A, B), "newName");            // swap a span
R.RemoveText(CharSourceRange::getTokenRange(Range));    // delete a span
```

The complete surface is small and composable: `InsertTextBefore` /
`InsertTextAfter` / `InsertTextAfterToken` for the point operations,
`ReplaceText` and `RemoveText` (both overloaded on `SourceLocation+length`,
`SourceRange`, and `CharSourceRange`), and `getRewrittenText(Range)` to read
back a span *with edits applied*. Crucially, the `Rewriter` maps edits through
the SourceManager, so a `SourceRange` you pulled off any AST node in Parts
3–29 is a valid edit target — the location discipline from Part 31 is exactly
what you need here. Edits at distinct offsets compose freely; the buffer keeps
them ordered.

## 38.2 — Getting the result out

Edits accumulate in a per-file `RewriteBuffer`. Two ways to realize them:

```cpp
// (a) Inspect — replay the main file's buffer to any stream (lab-safe):
FileID FID = SM.getMainFileID();
if (const llvm::RewriteBuffer *Buf = R.getRewriteBufferFor(FID))
  Buf->write(llvm::outs());

// (b) Commit — write every changed buffer back to disk in place:
R.overwriteChangedFiles();   // returns true if any write FAILED
```

The skeleton uses (a) — it prints to stdout and never touches your files,
which is the right default while you're developing an edit. Flip to (b) only
once the edits are trustworthy. Run it now and the marker comment lands before
every function *definition* in the main file:

```
class Shape {
public:
  /* [rewritten] */ explicit Shape(std::string name) : name_(std::move(name)) {}
  /* [rewritten] */ virtual ~Shape() = default;
  virtual double area() const = 0;
  /* [rewritten] */ const std::string &name() const { return name_; }
```

Note `area()` — a pure-virtual *declaration*, not a definition — is correctly
skipped, because the visitor gates on `isThisDeclarationADefinition()`.

## 38.3 — Replacements: the serializable tier

`Rewriter` is stateful and single-process. Production refactoring tools need
edits that can be *collected, deduplicated, and applied later* — possibly from
many TUs that each saw the same header. That's `tooling::Replacement` /
`tooling::Replacements` (`clang/Tooling/Core/Replacement.h`):

```cpp
#include "clang/Tooling/Core/Replacement.h"

tooling::Replacement Rep(SM, CharSourceRange::getTokenRange(Range), "override");
llvm::Error Err = Replacements.add(Rep);   // rejects overlaps as an error
```

A `Replacement` is a *value* — `(file, offset, length, replacement text)` —
independent of any live AST, so it serializes to YAML and merges across runs.
`Replacements::add` refuses overlapping edits (the multi-TU dedup problem
Part 35 flagged), and `RefactoringTool` — a `ClangTool` subclass — collects
them from every TU and applies the merged set. This is the tier `clang-tidy`
fixes and `clang-apply-replacements` run on.

## 38.4 — The Edit library

One tier lower sits `clang/Edit` — `Commit` + `EditedSource` — the atomic,
conflict-checked edit engine that Clang's own ARC/ObjC migrators are built on:

```cpp
#include "clang/Edit/Commit.h"
#include "clang/Edit/EditedSource.h"

EditedSource Editor(SM, LangOpts);
Commit C(Editor);
C.insertBefore(Loc, "const ");
C.replace(CharSourceRange::getTokenRange(R), "auto");
if (C.isCommitable())      // all-or-nothing: a bad location fails the batch
  Editor.commit(C);        // apply the group atomically
Editor.applyRewrites(Receiver);   // stream results to an EditsReceiver
```

A `Commit` is a *transaction*: several edits that either all apply or none do,
with token-boundary validation the raw `Rewriter` doesn't perform. Most tools
don't need this tier — reach for it when a single logical change is several
edits that must not partially land.

**Quiz.** The skeleton inserts its marker with
`R.InsertText(FD->getBeginLoc(), …)`. Suppose instead you wanted to append
`override` to a method whose body starts at `area() const { … }`. Why is
`InsertTextAfterToken(methodEndLoc, …)` treacherous when the method is written
using a macro, and which Part-31 idea rescues you?

> [!hint]- Hint
> `getBeginLoc()`/`getEndLoc()` can point *inside* a macro expansion. What
> does the SourceManager say a macro-expansion location's "spelling" is, and
> can you edit that?

> [!success]- Answer
> An edit location that falls in a macro *expansion* has no directly writable
> spot in the user's source — the text it stands for lives in the macro
> definition, possibly in a system header, and the same expansion may occur
> many times. Writing there either fails or corrupts an unrelated use. The fix
> is Part 31's spelling-vs-expansion distinction: test
> `SM.isMacroBodyExpansion(Loc)` / `isMacroArgExpansion(Loc)` and either map
> to the spelling loc with `SM.getSpellingLoc` or refuse the edit. Robust
> rewriters validate every target location before emitting — which is exactly
> why the `Replacements` and `Commit` tiers reject non-file locations for you.

## 38.5 — Hands-on: an `override` inserter

Turn the marker into something real. The change is one line of logic in the
visitor: instead of marking every definition, find `CXXMethodDecl`s that
override a base method but lack the `override` keyword, and insert it.

```cpp
bool VisitCXXMethodDecl(CXXMethodDecl *M) {
  if (M->size_overridden_methods() == 0)      // not an override
    return true;
  if (M->hasAttr<OverrideAttr>() || M->hasAttr<FinalAttr>())
    return true;                              // already written
  // Insert before the body/semicolon — after the declarator:
  SourceLocation End = M->getFunctionTypeLoc().getEndLoc();
  R.InsertTextAfterToken(End, " override");
  return true;
}
```

On the sample, `Circle::area` already has `override`, so a correct
implementation leaves it alone and reports nothing to change — the honest
result, and the same edge case (don't double-insert) every real modernizer
handles. This *is* clang-tidy's `modernize-use-override`, minus the diagnostic
wrapper; graduate it to `Replacements` (§38.3) and it would run project-wide
under `clang-apply-replacements`. The next part drops back to reading —
control-flow and call-graph analysis over the same AST.
