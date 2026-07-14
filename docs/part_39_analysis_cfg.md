# Part 39 — Analysis: the CFG, CallGraph & Flow Analyses

[← Part 38 — Rewriting & Edits](part_38_rewrite_edit.md) | [README](README.md)

The AST tells you what the code *says*; it does not tell you how control
*flows* through it. "Is this block reachable?", "is this variable live here?",
"can this function return without a value?" are questions about paths, and
paths aren't AST edges — an `IfStmt` is one node whether or not its branches
rejoin. `clang/Analysis` answers them by lowering a function body into a
**control-flow graph**: basic blocks (straight-line runs of statements) joined
by edges (branches, loops, short-circuits). This is the same substrate the
Clang Static Analyzer and clang-tidy's flow-sensitive checks run on.

The sixth skeleton, `skeletons/cfg-tool/`, is wired for it:

```bash
cd skeletons/cfg-tool
cmake -G Ninja -B build -DCMAKE_PREFIX_PATH=$(brew --prefix llvm) \
  -DCMAKE_CXX_COMPILER=$(brew --prefix llvm)/bin/clang++ .
cmake --build build
./build/cfg-tool sample/sample.cpp -- -std=c++17
```

## 39.1 — Building a CFG

A CFG is *not* part of the AST — it's built on demand, per function body, and
owned by you:

```cpp
#include "clang/Analysis/CFG.h"

std::unique_ptr<CFG> Graph =
    CFG::buildCFG(FD, FD->getBody(), &Ctx, CFG::BuildOptions());
```

`buildCFG` walks the body's statements and splits them at every control
transfer. `BuildOptions` toggles what gets modeled — implicit destructor
calls, temporary lifetimes, `&&`/`||` short-circuit branches — because a
lifetime checker and a reachability checker want different levels of detail.
The result is null only for a body it can't model; a normal function always
builds.

## 39.2 — Blocks, terminators, and edges

A `CFG` is a container of `CFGBlock`s. Each block has an ID, a list of AST
statement elements, an optional **terminator** (the `if`/`while`/`switch` that
decided where to branch), and iterators over its predecessor and successor
edges:

```cpp
for (const CFGBlock *B : *Graph) {
  B->getBlockID();                  // stable per-CFG id
  B->getTerminatorStmt();           // the branch that ends this block, or null
  for (const CFGBlock *Succ : B->succs())
    if (Succ) { /* an edge; Succ can be null = statically unreachable */ }
}
```

Two structural invariants matter. `getEntry()` is where flow starts and
`getExit()` is the single sink every path funnels into — the exit is block
**B0**, the entry is the *highest*-numbered block (blocks are numbered in
reverse post-order). And a successor slot can be **null**: that's Clang
telling you an edge exists in the language but is statically unreachable
(a `while(false)` body, a branch after a `[[noreturn]]` call).

The skeleton prints each function's blocks and successor edges. The
straight-line methods are trivial (`Shape::Shape: 2 blocks`), but the branchy
ones show real structure. `geo::clamp` — two `if`s and a conditional
operator — lowers to eight blocks:

```
geo::clamp: 8 blocks
  B0 ->            (exit)
  B1 -> B0
  B2 -> B1
  B3 -> B1
  B4 -> B2 B3      (the ?: — two-way branch)
  B5 -> B0
  B6 -> B5 B4      (the first if — two-way branch)
  B7 -> B6         (entry)
```

Blocks `B6` and `B4` each have two successors — those are the two-way
branches (`if (value < lo)` and the `? :`); the single-successor blocks are
straight-line joins. `totalArea`, with its range-`for`, shows the shape that
gives loops away — a **back-edge**:

```
totalArea: 7 blocks
  B2 -> B4 B1      B2 is the loop condition: exit to B1, or body...
  B3 -> B2         ...and the body's end jumps BACK to B2
  B4 -> B3
```

`B3 -> B2` is control returning to the condition after the body — the
signature every loop leaves in a CFG, and what a loop-detection or
termination analysis keys on.

## 39.3 — CallGraph: who calls whom

Where the CFG is intra-procedural (one body), `CallGraph`
(`clang/Analysis/CallGraph.h`) is inter-procedural — it walks the whole AST
and records, for each function, the functions it calls:

```cpp
#include "clang/Analysis/CallGraph.h"

CallGraph CG;
CG.addToCallGraph(Ctx.getTranslationUnitDecl());
for (const CallGraphNode *N : *CG.getRoot()) {
  const auto *FD = llvm::dyn_cast_or_null<FunctionDecl>(N->getDecl());
  N->size();          // number of callees
}
```

One caveat the skeleton handles for you: `CallGraph` indexes *every* function
it can reach, including the `std` internals the headers instantiate, so it
filters callers down to the main file. The result on the sample is the
program's real static call structure:

```
totalArea calls 6 function(s)
main calls 5 function(s)
main()::(lambda)::operator() calls 0 function(s)
```

Each `CallGraphNode` also exposes `callees()` to walk edges, so the graph is
traversable both ways — the basis for reachability ("is this function ever
called?"), cycle detection (recursion), and topological orderings.

## 39.4 — The prebuilt analyses

You rarely walk raw blocks by hand; `clang/Analysis/Analyses/` ships the
flow-sensitive analyses tools actually consume, each taking a built CFG:

| Analysis | Header | Answers |
|----------|--------|---------|
| Dominators | `Dominators.h` | which blocks must execute before this one |
| Live variables | `LiveVariables.h` | is this variable's value still needed here |
| Uninitialized values | `UninitializedValues.h` | is this read possibly of an uninitialized var |
| Reachable code | `ReachableCode.h` | which statements can never execute (`-Wunreachable-code`) |
| CFG reachability | `CFGReachabilityAnalysis.h` | can block A reach block B along any path |
| Thread safety | `ThreadSafety.h` | is this access properly guarded (`-Wthread-safety`) |

These aren't exotic — they *are* the compiler warnings. `-Wuninitialized`,
`-Wunreachable-code`, and `-Wthread-safety` are these analyses wired to
diagnostics. Running one is "build the CFG, hand it to the analysis, read the
result", and the AnalysisDeclContext (`AnalysisDeclContext.h`) caches the CFG
so several analyses share one build.

**Quiz.** The skeleton prints a successor edge only `if (Succ)` — skipping
null slots. If you were writing an *unreachable-code* finder, would ignoring
those null successors help you or hide the very thing you're hunting? And what
is the actual set of unreachable blocks — how do you compute it from the
graph?

> [!hint]- Hint
> A null *successor* and an unreachable *block* are different objects. Which
> one is "code that can't run"? How would you discover every such block
> starting from `getEntry()`?

> [!success]- Answer
> The two are unrelated, and conflating them is the trap. A null successor
> *slot* marks one edge the language allows but that is statically dead — a
> useful hint, but not the answer. An unreachable *block* is one no path from
> the entry reaches, and you find the whole set structurally: do a
> graph search (BFS/DFS) from `getEntry()` following `succs()`, collect every
> visited block, and the unreachable set is `all blocks − visited`. Skipping
> null successors doesn't hurt that search (a null edge leads nowhere to
> visit), but the *reachable* set — not the null edges — is what tells you
> what's dead. This is precisely what `reachable_code.h` industrializes, with
> the extra care of not reporting compiler-synthesized or intentionally-dead
> blocks (like the `false` arm of a `static_assert`-style constant branch).

## 39.5 — Hands-on: an unreachable-block finder

Put the quiz's answer in code. Add a reachability sweep to the skeleton's
`VisitFunctionDecl`, right after the CFG is built:

```cpp
llvm::DenseSet<const CFGBlock *> Reachable;
llvm::SmallVector<const CFGBlock *> Work{&Graph->getEntry()};
while (!Work.empty()) {
  const CFGBlock *B = Work.pop_back_val();
  if (!Reachable.insert(B).second) continue;
  for (const CFGBlock *S : B->succs())
    if (S) Work.push_back(S);
}
for (const CFGBlock *B : *Graph)
  if (!Reachable.count(B) && B->size() > 0)     // has real statements
    llvm::outs() << "  unreachable: B" << B->getBlockID() << "\n";
```

On the clean sample this reports nothing — there's no dead code to find, the
honest result. To see it fire, drop a `return;` before a statement in a body
and rebuild: the statements past the `return` land in a block the entry can no
longer reach, and your finder names it. You've just written the core of
`-Wunreachable-code` from first principles — and with Part 38's `Rewriter`,
the obvious next tool writes itself: find the dead blocks, then *delete* their
source ranges.

---

That closes the analysis layer, and the lab's guided path. From here the three
production frameworks — the Static Analyzer (symbolic execution over these
CFGs), clang-tidy (matcher + fix-it + diagnostic), and clangd (a server over
`ASTUnit`s and the Index feed from Part 37) — are assemblies of exactly the
pieces you've now handled directly.
