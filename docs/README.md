# LibTooling Lab — Hands-on Clang LibTooling (C++)

A hands-on lab for learning **Clang LibTooling** — the C++ library for building
standalone source tools (linters, analyzers, refactoring tools) on top of
Clang's full front-end. The lab digs into the machinery itself: the AST and
every major node family, visitors, the source manager, the lexer and tokens,
matchers, and the tooling command-line APIs.

> This is the **C++ in-tree LibTooling API**, deliberately distinct from the
> Python `libclang` bindings covered in the sibling `libclang-lab`.

## Environment — fully local

Everything builds and runs **on your Mac**. Homebrew's LLVM ships the complete
LibTooling development kit (headers, static/dynamic libs, CMake config,
`clang-query`).

| Component | Details |
|-----------|---------|
| OS | macOS (Apple Silicon or Intel) |
| Toolchain | `brew install llvm cmake ninja` — LLVM 22 at `$(brew --prefix llvm)` |
| Compiler for the tools | `$(brew --prefix llvm)/bin/clang++` (not Apple clang — it lacks the dev headers) |
| Build | CMake ≥ 3.20 + Ninja |

Part 1 covers setup. The skeletons resolve all platform header paths at
runtime from the installed LLVM, so every tool run in the lab is simply
`<tool> <file> -- -std=c++17` — nothing else to install or pass, nothing
that fails.

## Lab Parts

The AST deep dive (Parts 3–29) covers **every concrete node kind Clang
defines** — all 91 declaration kinds, all 259 statement/expression kinds, and
all 59 type kinds, as extracted from this LLVM's own `DeclNodes.inc` /
`StmtNodes.inc` / `TypeNodes.inc`. Majors get full sections; the tail
(including ObjC/OpenMP/OpenACC, out of scope for C++) is always listed, never
omitted. [`node_index.md`](node_index.md) maps every kind to its lesson.

**Foundations**

| # | Part | Topics |
|---|------|--------|
| 1 | [Setup & Your First Tool](part_1_setup_first_tool.md) | Clang's three interfaces · brew LLVM · the skeleton projects · `ClangTool` wiring · first run |
| 2 | [AST Fundamentals](part_2_ast_fundamentals.md) | `-ast-dump` · `ASTContext` · the hierarchies · `dyn_cast`/`DynTypedNode` · the exploration workflow |

**Declarations — all 91 kinds**

| # | Part | Topics |
|---|------|--------|
| 3 | [Infrastructure](part_3_decls_infrastructure.md) | `Decl` base · `DeclarationName` · `DeclContext` · redeclaration chains · the kind map |
| 4 | [Variables & Fields](part_4_decls_variables.md) | `VarDecl` · `ParmVarDecl` · `ImplicitParamDecl` · `FieldDecl` · `IndirectFieldDecl` · structured bindings |
| 5 | [Functions & Methods](part_5_decls_functions.md) | `FunctionDecl` in depth · `CXXMethodDecl` · ctors/dtors/conversions · deduction guides |
| 6 | [Records](part_6_decls_records.md) | `RecordDecl`/`CXXRecordDecl` · bases · access · friends · record layout |
| 7 | [Templates I](part_7_decls_templates_functions.md) | template parameter decls · `FunctionTemplateDecl` · pattern vs instantiation |
| 8 | [Templates II](part_8_decls_templates_classes.md) | class/variable/alias templates · specializations · concepts |
| 9 | [Enums, Aliases & Scopes](part_9_decls_enums_scopes.md) | enums · typedefs/aliases · namespaces · the whole `using` family |
| 10 | [The Long Tail](part_10_decls_long_tail.md) | every remaining decl kind: asserts, linkage, blocks, pragmas, ObjC/OMP tables |

**Statements — every kind**

| # | Part | Topics |
|---|------|--------|
| 11 | [Core & Selection](part_11_stmts_core.md) | `CompoundStmt` · `DeclStmt` · labels/attributes · `IfStmt` · `SwitchStmt` machinery |
| 12 | [Loops & Jumps](part_12_stmts_loops_jumps.md) | all four loops · range-for desugaring · return/break/continue/goto |
| 13 | [The Long Tail](part_13_stmts_long_tail.md) | try/catch · SEH · asm · coroutine statements · full OMP/OpenACC/ObjC tables |

**Expressions — every kind**

| # | Part | Topics |
|---|------|--------|
| 14 | [The Expr Base](part_14_expr_base.md) | `getType` · value categories · object kinds · classification |
| 15 | [Literals](part_15_expr_literals.md) | every literal kind, `APInt`/`APFloat`, UDLs, compound literals |
| 16 | [Names & Members](part_16_expr_names_members.md) | `DeclRefExpr` · `MemberExpr` · `__func__` · `source_location` |
| 17 | [Operators](part_17_expr_operators.md) | unary/binary/conditional · rewritten `<=>` · subscripts · sizeof/offsetof · atomics |
| 18 | [Calls](part_18_expr_calls.md) | `CallExpr` family · member & operator calls · builtins |
| 19 | [Construction & Init](part_19_expr_construction_init.md) | `CXXConstructExpr` · `InitListExpr` syntactic/semantic · designated init · defaults |
| 20 | [Casts](part_20_expr_casts.md) | `ImplicitCastExpr` · the CastKind taxonomy · all explicit cast kinds |
| 21 | [C++ Objects](part_21_expr_cpp_objects.md) | this/new/delete · typeid · throw/noexcept · lambdas · type traits |
| 22 | [The Invisible AST](part_22_expr_invisible.md) | temporaries · cleanups · `OpaqueValueExpr` · `RecoveryExpr` · `Ignore*` helpers |
| 23 | [Dependence & Packs](part_23_expr_dependent_packs.md) | unresolved/dependent nodes · pack expansion · folds · requires/concepts |
| 24 | [Constant Eval & Misc](part_24_expr_consteval_misc.md) | `EvaluateAs*`/`APValue` · coroutine exprs · `_Generic` · the complete remaining tail |

**Types — all 59 kinds**

| # | Part | Topics |
|---|------|--------|
| 25 | [QualType & Builtins](part_25_types_qualtype_builtins.md) | qualifier model · the full `BuiltinType` zoo |
| 26 | [Derived Types](part_26_types_derived.md) | pointers/references · all array kinds · vectors/matrices · atomics · `_BitInt` |
| 27 | [Function, Tag & Sugar](part_27_types_function_tag_sugar.md) | `FunctionProtoType` · records/enums · every sugar node · `typeof` |
| 28 | [Deduction & Dependence](part_28_types_deduction_dependence.md) | `auto`/CTAD/`decltype` · template specialization types · dependent types |
| 29 | [TypeLoc](part_29_types_typeloc.md) | `TypeSourceInfo` · the TypeLoc hierarchy · as-written type locations |

**Working the tree**

| # | Part | Topics |
|---|------|--------|
| 30 | [Visitors](part_30_visitors.md) | `RecursiveASTVisitor` · `Visit`/`Traverse`/`WalkUpFrom` · traversal control · `DynamicRecursiveASTVisitor` · parents & context |
| 31 | [SourceManager & Locations](part_31_source_manager.md) | `SourceLocation` internals · spelling vs expansion (macros) · ranges · extracting source text · file filters |
| 32 | [Tokens & the Lexer](part_32_tokens_lexing.md) | the token stream · `Token` API · raw lexing · comments · `Lexer` static helpers |
| 33 | [AST Matchers](part_33_ast_matchers.md) | matcher DSL · `clang-query` prototyping · `MatchFinder` · binding · composition · traversal modes |
| 34 | [Command-Line APIs](part_34_command_line.md) | `llvm::cl` · custom options · `OptionCategory` · `CommonOptionsParser` · compilation databases |
| 35 | [Advanced Tooling](part_35_advanced.md) | `ArgumentsAdjusters` · virtual files · `runToolOnCode` · `ASTUnit` · multi-TU execution · where to go next |
| 36 | [The Preprocessor](part_36_preprocessor_includes.md) | `PPCallbacks` · the include list & include graph · macro definitions/expansions · skipped regions · `PreprocessOnlyAction` |

**Libraries beyond the AST**

| # | Part | Topics |
|---|------|--------|
| 37 | [The Index Library](part_37_index.md) | USRs (cross-TU identity) · `SymbolKind` & roles · `IndexDataConsumer` · the occurrence stream · `createIndexingAction` |
| 38 | [Rewriting & Edits](part_38_rewrite_edit.md) | `Rewriter` (Insert/Replace/Remove) · `RewriteBuffer` · `Replacements`/`RefactoringTool` · the `Edit` library (`Commit`/`EditedSource`) |
| 39 | [Analysis: the CFG](part_39_analysis_cfg.md) | `CFG::buildCFG` · blocks, terminators & edges · `CallGraph` · dominators/live-vars/reachable-code · an unreachable-block finder |

## Repository Layout

```
libtooling-lab/
├── docs/                    ← these lab docs (start here)
└── skeletons/
    ├── visitor-tool/        ← ClangTool + RecursiveASTVisitor skeleton (most parts)
    ├── matcher-tool/        ← ClangTool + MatchFinder skeleton (Part 33)
    ├── index-tool/          ← clang/Index occurrence lister (Part 37)
    ├── rewrite-tool/        ← clang/Rewrite source rewriter (Part 38)
    └── cfg-tool/            ← clang/Analysis CFG + CallGraph (Part 39)
```

Each skeleton is `CMakeLists.txt` + `main.cpp` + `sample/sample.cpp`. All six
compile as-is; the lab's hands-on work is editing them. The shared
`sample/sample.cpp` is a deliberately dense translation unit (namespaces,
inheritance, templates, lambdas, macros) that all parts analyze.

## Prerequisites

- Homebrew
- Basic C++ (classes, virtual functions, templates at a reading level)
- No prior LLVM/Clang internals experience needed

## Track Your Progress

See [PROGRESS.md](PROGRESS.md) — mark sections `[x]` as you go.
