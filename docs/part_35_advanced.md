# Part 35 — Advanced Tooling

[← Part 34 — Command-Line APIs](part_34_command_line.md) | [Part 36 — The Preprocessor →](part_36_preprocessor_includes.md)

Everything so far ran one `ClangTool` over files on disk. This part covers
the machinery around that core: adjusting arguments programmatically
(including the adjuster the skeletons have been quietly using since Part 1),
feeding the tool code that isn't on disk, getting an AST without the action
ceremony, and scaling to many translation units.

## 35.1 — ArgumentsAdjusters

An `ArgumentsAdjuster` is a function that rewrites each compile command just
before the front-end runs — the programmatic version of `--extra-arg`. This
is how a tool bakes in platform knowledge so its users don't have to:

```cpp
#include "clang/Tooling/ArgumentsAdjusters.h"

ClangTool Tool(Options->getCompilations(), Options->getSourcePathList());
Tool.appendArgumentsAdjuster(getInsertArgumentAdjuster(
    "-resource-dir=/opt/homebrew/opt/llvm/lib/clang/22",
    ArgumentInsertPosition::END));
```

Stock adjusters: `getInsertArgumentAdjuster` (one flag or a list, at `BEGIN`
or `END`), `getClangStripOutputAdjuster` (drops `-o …`),
`getClangSyntaxOnlyAdjuster` (turns full compiles into `-fsyntax-only` —
`ClangTool` installs this one by default, which is why your tools never
generate object files even when the database says `-c`). An adjuster is just
`std::function<CommandLineArguments(const CommandLineArguments&, StringRef
File)>`, so arbitrary surgery — filtering out flags your Clang version
doesn't know, redirecting include paths — is a lambda away.

You've been relying on this mechanism since Part 1: the skeletons' `main()`
appends `-isysroot` and `-resource-dir` (values CMake discovered at
configure time) through exactly these calls — that's why a standalone
binary in `build/` resolves the SDK and builtin headers as smoothly as a
real `clang` does. Reread the end of `main.cpp` with fresh eyes; it should
now be fully transparent.

## 35.2 — Virtual files and runToolOnCode

Two facilities remove the filesystem from the equation entirely.

**Mapped files** — inject in-memory content under any path:

```cpp
Tool.mapVirtualFile("/fake/config.h", "#define LAB_MODE 1\n");
```

The front-end will "find" `/fake/config.h` in includes. Use it to stub
headers you don't have, or pin a header to known content in tests.

**`runToolOnCode`** — skip `ClangTool` and databases altogether, parse a
string:

```cpp
#include "clang/Tooling/Tooling.h"

bool Ok = runToolOnCodeWithArgs(
    std::make_unique<MyAction>(),
    "struct S { virtual void f(); };  struct T : S { void f(); };",
    {"-std=c++17"},
    "input.cc");
```

This is the unit-testing backbone for tool logic: every clang-tidy check's
test suite is essentially assertions over `runToolOnCodeWithArgs` calls with
tiny snippets. No SDK flags needed — a self-contained snippet with no
`#include`s touches neither the standard library nor the builtin headers.
Write your check's edge cases as snippets and your feedback loop drops from
"rebuild + rerun on sample.cpp" to a `ctest` run.

## 35.3 — ASTUnit: an AST without an action

Sometimes the Action/Consumer ceremony is pure overhead — you just want an
AST *object* to poke at, right here in `main`:

```cpp
#include "clang/Frontend/ASTUnit.h"

std::unique_ptr<ASTUnit> Unit =
    buildASTFromCodeWithArgs("struct S { int x; };", {"-std=c++17"});
ASTContext &Ctx = Unit->getASTContext();
// visitors, matchers, node APIs — everything from Parts 2–33 works on Ctx
```

`ASTUnit` owns the whole parse (context, source manager, diagnostics) as a
value you can store, pass around, or keep several of — which is what IDEs
and `clangd` do. The heavyweight sibling is
`ClangTool::buildASTs(std::vector<std::unique_ptr<ASTUnit>>&)`, which
materializes one `ASTUnit` per TU in the database — convenient for
cross-TU analysis, at the cost of holding every AST in memory at once.

**Quiz.** With `buildASTs` you could load all of a project's ASTs and search
across them. Name the two resources that make this strategy collapse on a
2,000-TU codebase, and the header in `clang/Tooling/` that solves it.

> [!success]- Answer
> Memory (an AST is routinely hundreds of MB per TU — headers are re-parsed
> into *every* unit) and time (single-threaded sequential parses). The
> scalable alternative is streaming execution per TU with parallelism:
> `AllTUsExecution.h` — next section.

## 35.4 — Running over many files

`ClangTool` processes its file list sequentially in one process. For
project-scale runs there's `AllTUsToolExecutor`, which runs your
`FrontendActionFactory` over **every TU in the compilation database on a
thread pool**:

```cpp
#include "clang/Tooling/AllTUsExecution.h"

AllTUsToolExecutor Executor(Options->getCompilations(), /*ThreadCount=*/0);
auto Err = Executor.execute(newFrontendActionFactory<MyAction>());
```

The execution model changes more than speed. Callbacks now run
concurrently — shared state needs a mutex, and results should go through
the executor's `ExecutionContext`/`ToolResults` (a thread-safe key-value
sink) rather than `llvm::outs()` interleaving. And entities that appear in
many TUs (every method of a header-defined class, one per includer) now get
*reported* once per TU, so real multi-TU tools deduplicate findings by
`(file, offset, check)` — the same reason clang-tidy runs under
`run-clang-tidy.py` deduplicate their replacements.

For most lab-scale work, plain `ClangTool` + a loop is fine; know where the
ceiling is and what's above it.

## 35.5 — Where to go next

A closing exercise that ties §35.1 and §35.2 together: revisit the
skeleton's `addPlatformFlags()` — it already resolves the resource dir and
SDK at runtime (via `dladdr` and `xcrun`). Harden it: replace the `popen`
call with `llvm::sys::ExecuteAndWait`, honor an `SDKROOT` environment
variable as an override before falling back to `xcrun`, and prove the
binary is relocatable by moving it to another directory and running it
there. Then write its first regression test as a `runToolOnCodeWithArgs`
snippet.

From here, the natural next layers, in learning order:

1. **Rewriting** — `Rewriter` (`clang/Rewrite/Core/Rewriter.h`):
   `InsertText`/`ReplaceText`/`RemoveText` against the SourceManager you
   already know; then the structured layer, `Replacements` +
   `RefactoringTool`, which serializes edits and merges duplicates.
2. **Diagnostics** — emit real compiler-style warnings with
   `DiagnosticsEngine` and attach `FixItHint`s; with Part 31's location
   discipline you already know where they should point.
3. **A clang-tidy check** — the productized form of everything here:
   matcher + callback + diagnostic + fix-it, hosted inside clang-tidy's
   framework. Reading one real check
   (`clang-tools-extra/clang-tidy/modernize/UseOverrideCheck.cpp`) after
   this lab is a very different experience than reading it before.
4. **clangd / libclang** — the same AST through other doors: a long-lived
   server over `ASTUnit`s, and the stable C API from the sibling
   `libclang-lab`.
