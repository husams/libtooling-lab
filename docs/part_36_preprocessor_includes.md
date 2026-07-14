# Part 36 — The Preprocessor: Includes, Macros & Callbacks

[← Part 35 — Advanced Tooling](part_35_advanced.md) | [Part 37 — The Index Library →](part_37_index.md)

Everything the AST parts taught happens *after* the preprocessor has run:
includes are already spliced in, macros already expanded, `#if 0` regions
already gone. When the question is about the directives themselves — *which
files does this TU include? who includes whom? what macros does this file
define?* — the AST is the wrong layer. The preprocessor will tell you
directly, live, as it works: you subscribe to it with `PPCallbacks`.

## 36.1 — Getting hold of the Preprocessor

The `Preprocessor` object lives on the `CompilerInstance`, and the natural
place to reach it is the same `CreateASTConsumer` hook the skeletons already
override:

```cpp
#include "clang/Lex/PPCallbacks.h"
#include "clang/Lex/Preprocessor.h"

class Action : public ASTFrontendAction {
public:
  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI,
                                                 StringRef) override {
    CI.getPreprocessor().addPPCallbacks(
        std::make_unique<IncludeLister>(CI.getSourceManager()));
    return std::make_unique<ASTConsumer>();   // empty consumer is fine
  }
};
```

You can register any number of callback objects; the preprocessor owns them
(`unique_ptr`) and chains them.

**Quiz.** `CreateASTConsumer` runs before a single line is preprocessed —
yet it's the right place to register preprocessor callbacks. Why does that
work at all?

> [!hint]- Hint
> When does Clang actually preprocess? Is it a separate pass that finishes
> before parsing starts?

> [!success]- Answer
> There is no separate preprocessing pass: the parser pulls tokens on
> demand, and the preprocessor expands includes/macros lazily as tokens are
> requested. `CreateASTConsumer` runs at file setup, before the token pump
> starts, so callbacks registered there see every directive of the whole TU
> as parsing proceeds.

## 36.2 — PPCallbacks: the hook surface

`PPCallbacks` (`clang/Lex/PPCallbacks.h`) is a virtual interface with a hook
for essentially every preprocessor event. The ones tools actually use:

| Callback | Fires on |
|----------|----------|
| `InclusionDirective` | every `#include`/`#import` — the include list |
| `FileChanged` | entering/leaving a file (the live include *stack*) |
| `MacroDefined` / `MacroUndefined` | `#define` / `#undef` |
| `MacroExpands` | every macro *use* about to be expanded |
| `Defined`, `If`, `Ifdef`, `Elif`, `Else`, `Endif` | conditional directives, with the condition's evaluated value |
| `SourceRangeSkipped` | a region eliminated by `#if 0` / failed conditional |
| `PragmaDirective` (+ specific `Pragma*` hooks) | `#pragma` |
| `EndOfMainFile` | preprocessing finished — your "print the report" moment |

Default implementations are empty, so you override only what you need.

## 36.3 — Hands-on: an include lister

The workhorse hook. Its parameters hand you everything about one directive:
the spelled name, `<>` vs `""`, and — crucially — the **resolved file** the
header search actually chose:

```cpp
class IncludeLister : public PPCallbacks {
public:
  explicit IncludeLister(SourceManager &SM) : SM(SM) {}

  void InclusionDirective(SourceLocation HashLoc, const Token &IncludeTok,
                          StringRef FileName, bool IsAngled,
                          CharSourceRange FilenameRange,
                          OptionalFileEntryRef File, StringRef SearchPath,
                          StringRef RelativePath, const Module *SuggestedModule,
                          bool ModuleImported,
                          SrcMgr::CharacteristicKind FileType) override {
    ++Total;
    if (SM.isInMainFile(HashLoc)) {                  // direct includes only
      llvm::outs() << "direct: " << (IsAngled ? "<" : "\"") << FileName
                   << (IsAngled ? ">" : "\"");
      if (File)                                       // resolution can fail
        llvm::outs() << "  resolved: " << File->getName();
      llvm::outs() << "\n";
    } else {
      PresumedLoc Includer = SM.getPresumedLoc(HashLoc);
      llvm::outs() << "transitive: " << FileName << "  included by "
                   << Includer.getFilename() << ":" << Includer.getLine()
                   << "\n";
    }
  }

  void EndOfMainFile() override {
    llvm::outs() << Total << " inclusion directives in the whole TU\n";
  }

private:
  SourceManager &SM;
  unsigned Total = 0;
};
```

Wired into the sample (transitive output truncated here), verified:

```
direct: <string>  resolved: …/MacOSX.sdk/usr/include/c++/v1/string
transitive: __algorithm/max.h  included by …/c++/v1/string:592
transitive: __algorithm/comp.h  included by …/c++/v1/__algorithm/max.h:12
direct: <vector>  resolved: …/MacOSX.sdk/usr/include/c++/v1/vector
4500 inclusion directives in the whole TU
```

Two `#include` lines in the sample; **4,500 directives in the TU** — the
same file-vs-TU lesson as Part 32's token counts, now at the header level.
The pieces compose into every include tool you've seen: filtering on
`SM.isInMainFile(HashLoc)` gives the direct-include list
(include-what-you-use's starting point); the `(includer, includee)` pairs
from `getPresumedLoc(HashLoc)` build the full include *graph* (what `clang
-H` prints); `FileType` distinguishes user headers from system headers; and
a null `File` means an unresolved include your tool can flag before the
compiler errors.

## 36.4 — Macros

Same subscription, two more hooks. Definitions:

```cpp
void MacroDefined(const Token &Name, const MacroDirective *MD) override {
  if (MD && SM.isWrittenInMainFile(MD->getLocation()))
    llvm::outs() << "macro: " << Name.getIdentifierInfo()->getName() << "\n";
}
```

On the sample this prints exactly `SQUARE` and `GREETING` — but only because
of the `isWrittenInMainFile` filter, and that choice is a real finding worth
remembering: Clang's ~360 predefined macros (`__clang__`, `__GNUC__`, …) are
injected through a pseudo-file named `<built-in>` that the *expansion-based*
`isInMainFile` considers part of the main file. Filter macro locations with
the *spelling-based* `isWrittenInMainFile` (Part 31's distinction, earning
its keep), or ask `MD->getMacroInfo()->isBuiltinMacro()`.

For macro *uses*, override `MacroExpands(const Token &Name, const
MacroDefinition &MD, SourceRange Range, const MacroArgs *Args)` — this is
where "find every expansion of `SQUARE`" lives, and the `Range` it hands you
is the use site that Part 31's expansion locations decode. After parsing
you can also query rather than subscribe: `PP.getMacroInfo(IdentInfo)`
returns the current definition (parameters, replacement tokens,
function-like or not), and `PP.macros()` iterates everything defined.

## 36.5 — Skipped regions, the include stack, and friends

Three lesser-known hooks round out what the AST can never show:

- **`SourceRangeSkipped`** — the only way to see code disabled by `#if 0`
  or a failed conditional. It never reaches the parser, so no AST node
  exists; dead-code and "configuration coverage" tools live on this hook.
- **`FileChanged`** — fires on every enter/exit of a file with the reason
  (`EnterFile`, `ExitFile`, `SystemHeaderPragma`…). Maintaining a stack in
  this callback gives you the include depth at any moment — how `clang -H`
  indents its output.
- **`If`/`Ifdef`/`Elif`** — each reports its condition and how it
  evaluated, which is how you'd audit "which config branches does this
  build actually take?".

Beyond callbacks, header-search itself is programmable:
`PP.getHeaderSearchInfo()` (`HeaderSearch`) exposes the search directories
and per-header info. And when a tool needs *only* preprocessing — dependency
scanners, include graphers — skip AST building entirely by driving the run
with a `PreprocessOnlyAction` instead of an `ASTFrontendAction`: same
callbacks, a fraction of the runtime. That's the design behind
`clang-scan-deps`, the production tool to study once this part clicks.
