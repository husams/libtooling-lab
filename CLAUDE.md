# LibTooling Lab — Agent Guide

This is a hands-on lab teaching **Clang LibTooling** (the C++ library for
building standalone Clang-based source tools). It is distinct from the Python
`libclang-lab` — different API, different language, different goals.

The lab runs **entirely locally on macOS** against Homebrew LLVM. No VM, no
remote host.

## Response Style (strict)

Default to a **single short sentence** per response. Only expand into
detail, lists, or code when the user explicitly asks for details — or when
presenting a lab section, which follows the flow below.

## How to Present This Lab

Present the lab **interactively, section by section**. Do NOT dump entire
parts at once.

### Flow
1. Check `docs/PROGRESS.md` to see where the user left off.
2. Present ONE section at a time, following the doc's natural prose — the
   docs deliberately have no "Why/What/Verify" scaffolding; don't add it
   back when presenting.
3. Explain concepts before commands. When a section has hands-on steps,
   offer to run them; wait for confirmation before executing.
4. **Quizzes:** several sections end with a quiz (`**Quiz.**` followed by
   collapsed Obsidian callouts `> [!hint]- Hint` / `> [!success]- Answer`).
   Ask the question and let the user answer before revealing anything.
   Offer the hint if they're stuck; only then the answer. Never skip a
   quiz, never answer it for them unprompted. Never use raw HTML
   (`<details>` etc.) when writing or editing lab docs — Obsidian callouts
   only.
5. Use extra code or diagrams only when they genuinely clarify — the lab
   style is natural flow, not checkpoint-and-recap.
6. After completing a part, update `docs/PROGRESS.md` (mark sections `[x]`)
   and ask whether to continue.

### Before Making Changes
Show what will change in the skeleton code and why, make the change, build,
and confirm the result together before moving on.

## Lab Structure

```
libtooling-lab/
├── CLAUDE.md                     ← this file
├── README.md                     ← root pointer to docs/
├── docs/
│   ├── README.md                 ← TOC + environment
│   ├── PROGRESS.md               ← section-by-section checklist
│   ├── node_index.md             ← every concrete node kind → covering part
│   ├── part_1 … part_2           ← foundations (setup, AST fundamentals)
│   ├── part_3 … part_10          ← Declarations: ALL 91 decl kinds
│   ├── part_11 … part_13         ← Statements: every statement kind
│   ├── part_14 … part_24         ← Expressions: every expression kind
│   ├── part_25 … part_29         ← Types: ALL 59 type kinds + TypeLoc
│   ├── part_30 … part_36         ← visitors, SourceManager, lexer,
│   │                                matchers, command line, advanced,
│   │                                preprocessor/includes (PPCallbacks)
│   └── part_37 … part_39         ← libraries beyond the AST: Index (USRs),
│                                    Rewrite/Edit, Analysis (CFG/CallGraph)
├── skeletons/                    ← the six starter projects the user grows
│   ├── visitor-tool/             ← main skeleton (most parts)
│   │   ├── CMakeLists.txt
│   │   ├── main.cpp
│   │   ├── sample/sample.cpp     ← dense sample TU all parts analyze
│   │   └── build/                ← Ninja build dir
│   ├── matcher-tool/             ← Part 33 (MatchFinder wiring)
│   ├── index-tool/              ← Part 37 (clang/Index occurrence stream)
│   ├── rewrite-tool/            ← Part 38 (clang/Rewrite source rewriting)
│   └── cfg-tool/                ← Part 39 (clang/Analysis CFG + CallGraph)
│                                   (each: CMakeLists.txt, main.cpp,
│                                    sample/sample.cpp, build/)
└── legacy/                       ← pre-restructure VM-era tools (reference
                                    only, not part of the current lab)
```

All six skeletons compile as-is; the lab consists of growing them. Keep lesson
code inside the skeletons — do not create new projects unless the user asks.

## Environment

| Item | Value |
|------|-------|
| Toolchain | Homebrew LLVM (`brew install llvm cmake ninja`), LLVM 22 at `$(brew --prefix llvm)` |
| Compiler for tools | `$(brew --prefix llvm)/bin/clang++` — **never Apple clang** (no LibTooling headers, ABI mismatch) |
| Build dirs | `skeletons/*/build/` |
| clang-query | `$(brew --prefix llvm)/bin/clang-query` |

### Common commands
```bash
export LLVM=$(brew --prefix llvm)

# Build a skeleton
cd skeletons/visitor-tool
cmake -G Ninja -B build -DCMAKE_PREFIX_PATH=$LLVM -DCMAKE_CXX_COMPILER=$LLVM/bin/clang++ .
cmake --build build

# Run it against the sample — no platform flags needed; the skeletons resolve
# the SDK/resource-dir paths at runtime via addPlatformFlags() in main.cpp
./build/visitor-tool sample/sample.cpp -- -std=c++17

# Run against a compilation database (works out of the box too)
./build/visitor-tool -p build main.cpp

# Prototype matchers interactively
$LLVM/bin/clang-query sample/sample.cpp -- -std=c++17
```

Nothing in the lab flow is expected to fail — the skeletons handle all
platform plumbing (SDK path, resource dir, RTTI, C-language enablement for
LLVM's CMake probes). Present commands as "this just works", not as
failure-then-fix.

### Troubleshooting (only if something unexpectedly breaks)
- **`'string' / 'stdarg.h' file not found` at tool runtime**: only possible
  in a project created *outside* the skeletons — copy the
  `addPlatformFlags()` helper from a skeleton `main.cpp` and call it on the
  `ClangTool` before `run()`.
- **`Option '<name>' registered more than once` + abort**: a custom
  `llvm::cl::opt` name collides with an option inside libLLVM. Rename it
  (Part 34 §34.1 teaches this).
- **Assertion `Name.isIdentifier()` abort**: code called
  `NamedDecl::getName()` on a constructor/destructor/operator. Use
  `getNameAsString()` (Part 3 §3.2 teaches this).
- After a brew LLVM major bump: reconfigure the build dirs from scratch and
  suspect C++ API drift (docs verified against LLVM 22).
