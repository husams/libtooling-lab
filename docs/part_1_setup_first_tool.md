# Part 1 — Setup & Your First Tool

[← README](README.md) | [Part 2 — AST Fundamentals →](part_2_ast_fundamentals.md)

LibTooling is the C++ library that lets your own program run the real Clang
front-end over source files and receive the complete AST — every declaration,
statement, type, and source location. `clang-tidy`, `clang-format`, and
`clang-rename` are all built on it. By the end of this part you'll have a
LibTooling executable building and running locally on your Mac.

## 1.1 — Clang's three interfaces, and where LibTooling fits

Clang exposes three programming interfaces, and picking the wrong one costs
days:

| Interface | What it is | Use it when |
|-----------|------------|-------------|
| **Clang plugin** | A dylib the compiler loads *during your build* (`-Xclang -load`) | Analysis must run on every compile, inside the build |
| **libclang** | Stable **C API**, cursor-based, with Python/Rust/Go bindings; deliberately incomplete for C++ | You need ABI stability or bindings, and only symbols/types/locations |
| **LibTooling** | The full **C++ API**: complete AST, matchers, rewriting. Not stable across LLVM majors | Standalone tools that need deep analysis or edits — **this lab** |

**Quiz.** You want a command-line tool that finds every `virtual` method
missing `override` across a project and can rewrite the code. Which interface?

> [!success]- Answer
> LibTooling — you need the full C++ AST (libclang can't see enough of C++
> method semantics reliably) and you're standalone, not inside a build
> (rules out a plugin).

## 1.2 — Install the toolchain

Apple's clang does **not** ship the LibTooling development headers, but
Homebrew's LLVM ships everything: headers, libraries, CMake packages, and the
helper binaries (`clang-query`, `clang-check`).

```bash
brew install llvm cmake ninja
```

Nothing needs to be on your `PATH`; the lab always addresses the keg directly.
Two locations matter — worth memorizing the shape:

```bash
brew --prefix llvm                # e.g. /opt/homebrew/opt/llvm
ls $(brew --prefix llvm)/lib/cmake         # llvm/ clang/ …   ← CMake finds these
ls $(brew --prefix llvm)/include/clang/Tooling   # the headers you'll include
```

For the rest of the lab, set this once per shell:

```bash
export LLVM=$(brew --prefix llvm)
```

## 1.3 — Tour the visitor-tool skeleton

The lab ships two skeleton projects under `skeletons/`. Both compile as-is;
the entire lab consists of growing them. Open
`skeletons/visitor-tool/main.cpp` and read it bottom-up — LibTooling wiring
makes the most sense in the order things happen:

```
main()
 └─ CommonOptionsParser   parses argv: source files, -p, and everything after "--"
     └─ ClangTool         one front-end run per source file
         └─ FrontendAction   per-file lifecycle hook (ours: ASTFrontendAction)
             └─ ASTConsumer     receives the parsed AST
                 └─ Visitor        walks it (empty for now — Part 3's job)
```

Five small classes, always the same shape. Every LibTooling tool you'll ever
read — including clang-tidy — is this pattern with more code in the leaves.

The `CMakeLists.txt` is equally reusable. The three lines that matter:

```cmake
find_package(Clang REQUIRED CONFIG)          # locates brew LLVM via CMAKE_PREFIX_PATH
if(NOT LLVM_ENABLE_RTTI)
  add_compile_options(-fno-rtti)             # must match how LLVM was built
endif()
target_link_libraries(visitor-tool PRIVATE clang-cpp LLVM)   # the two monolithic dylibs
```

One non-obvious line: `project(visitor_tool C CXX)` enables **C** as well,
because LLVM's own CMake modules run a C header probe — with C++ only, the
very first configure fails with a confusing `check_include_file` error.

## 1.4 — Build it

```bash
cd skeletons/visitor-tool
cmake -G Ninja -B build \
  -DCMAKE_PREFIX_PATH=$LLVM \
  -DCMAKE_CXX_COMPILER=$LLVM/bin/clang++ .
cmake --build build
```

We compile the tool with **brew's** `clang++`, not Apple's: the tool links
against brew's C++ libraries, and mixing standard-library ABIs between the
two toolchains is a reliable source of undebuggable crashes.

A successful build gives you `build/visitor-tool`. It links in most of a
compiler, so ~80 MB is normal.

## 1.5 — Run it

```bash
./build/visitor-tool sample/sample.cpp -- -std=c++17
```

Silence plus exit code 0 is success: the whole of `sample.cpp` (including
`<string>` and `<vector>`) was parsed into an AST, and our empty visitor
walked it without printing anything. That's the command shape for the whole
lab: `<tool> <file> -- <compile flags>`.

One design note on why this works. Your tool *contains* the Clang front-end,
and a front-end needs to locate the SDK and Clang's builtin headers — a real
`clang` binary finds them relative to its own path, which a tool binary in
`build/` can't do. The skeleton's `addPlatformFlags()` resolves both at
runtime from what's already installed — it asks the brew LLVM the binary is
linked against for its builtin-header dir, and `xcrun` for the SDK — and
attaches them to every compile command through an **ArgumentsAdjuster**.
Part 35 looks at that machinery properly; until then you never think about
it.

## 1.6 — What the `--` actually does

A LibTooling tool must parse each file **exactly** as the real compiler
would — same language standard, same include paths, same macros. Otherwise
you'd analyze a different program than the one that builds. So every tool
needs a *compilation database*: a mapping from file → compile flags.

The `--` on the command line is the simplest one, a
`FixedCompilationDatabase`: "for any file, the flags are whatever follows the
`--`". That's what `-- -std=c++17` has been doing.

For real projects there's `compile_commands.json` (generated by CMake) and
the `-p` flag — Part 34 covers that properly. Until then, `--` is all we
need: the samples are single self-contained files.

**Quiz.** You run a tool over a file that uses `std::optional`, but forget to
pass `-std=c++17` after the `--`. The tool doesn't crash — what happens
instead, and why is that arguably worse?

> [!success]- Answer
> The front-end parses the file under the default standard, reports errors
> (`no member named 'optional' in namespace 'std'` or similar), and produces
> an AST with *error recovery* nodes — declarations may be missing or typed
> as dependent. Your analysis silently runs over that mangled AST and
> produces wrong answers instead of failing loudly.
