# Part 11 — Statements: Core & Selection

[← Part 10 — Declarations: The Long Tail](part_10_decls_long_tail.md) | [Part 12 — Loops & Jumps →](part_12_stmts_loops_jumps.md)

Declarations say what exists; statements say what happens. The next three
parts cover every concrete `Stmt` kind Clang defines. This one takes the
backbone — blocks, declarations-in-statement-position, labels — and the two
selection statements, whose AST carries noticeably more machinery than their
syntax suggests.

## 11.1 — The Stmt base

Every statement derives from `Stmt`, and the base keeps a deliberately tiny
interface:

```cpp
S->getStmtClass();            // discriminator: Stmt::IfStmtClass, …
S->getStmtClassName();        // "IfStmt" — handy in debug output
S->getBeginLoc(); S->getEndLoc();
S->children();                // generic child iteration, in source order
S->dump();                    // print the subtree to stderr — your best friend
```

`children()` is the generic, role-blind view: it yields each child once, but
tells you nothing about *what* it is to its parent. Real code uses the
subclass accessors (`getCond()`, `getBody()`, …) and leaves `children()` to
generic tooling — it's what `RecursiveASTVisitor` is built on.

Two structural facts from Part 2 do the most work here. `Expr` **is-a**
`Stmt`, so expressions appear anywhere statements can. And there is no
"ExpressionStatement" wrapper: in `main`, the line `(void)result;` is just a
`CStyleCastExpr` sitting directly in the `CompoundStmt`. When you iterate a
block's children, be prepared for any statement *or* expression node.

Between `Stmt` and `Expr` sits one thin abstract layer worth knowing:
**`ValueStmt`** — "a statement that may have a value". It exists for a C++
corner: a GNU statement-expression (Part 17) yields the value of its last
statement, and a `LabelStmt` or `AttributedStmt` wrapped around an
expression must not hide that value. If you ever need "the expression this
statement amounts to", that's `ValueStmt::getExprStmt()`.

## 11.2 — CompoundStmt

**`CompoundStmt`** is `{ … }` — an ordered sequence of statements. Every
function definition owns exactly one as its body (even an empty `{}`), and
they nest arbitrarily:

```cpp
CS->body();        // iterable range of Stmt*
CS->size();        // number of statements
CS->body_front(); CS->body_back();
CS->getLBracLoc(); CS->getRBracLoc();   // the braces — useful for rewriting
```

A detail that saves confusion later: a `CompoundStmt` is a statement, so it
can appear *as* the `then` of an `if`, the body of a loop, or on its own as
a bare scope block. "Function body" is a role, not a node property — the
same class serves all of them, and only the parent tells you which role
you're looking at.

## 11.3 — DeclStmt

**`DeclStmt`** is the bridge back into Part 3's world: a declaration in
statement position. `double total = 0.0;` inside `totalArea` is a `DeclStmt`
wrapping a `VarDecl` — the same `VarDecl` class you'd find at file scope,
just housed differently. One subtlety inherited from C:

```cpp
DS->isSingleDecl();     // true: double total = 0.0;
DS->getSingleDecl();
DS->decls();            // int a = 1, b = 2;  → ONE DeclStmt, TWO VarDecls
```

Checks about "declarations of multiple variables on one line" are `DeclStmt`
checks, not `VarDecl` checks — each `VarDecl` looks innocent on its own.
And it's not only variables: a local `struct`, a `typedef`, a `using` alias,
or a `static_assert` inside a function body each arrive wrapped in a
`DeclStmt` too.

## 11.4 — NullStmt, LabelStmt, AttributedStmt

**`NullStmt`** is a lone `;`. It has no children and one famous failure
mode — `if (x);` — which makes it a favorite matcher target for linters.
`NS->hasLeadingEmptyMacro()` tells you whether it came from an empty macro
expansion (`FOO;` where `FOO` expands to nothing), the classic false-positive
source for "suspicious semicolon" checks.

**`LabelStmt`** attaches a name to a statement: `done: return n;`. The label
itself is backed by a `LabelDecl` (from Part 10's tail), and the statement
under the label is `getSubStmt()`. Every label is a real node even if
nothing ever jumps to it.

**`AttributedStmt`** wraps a statement that carries statement-level
attributes — `[[fallthrough]];`, `[[likely]] case 9:`, `[[assume(x > 0)]];`:

```cpp
AS->getAttrs();        // ArrayRef<const Attr*>
AS->getSubStmt();      // the real statement underneath
```

Verified shape from a case with `[[likely]]`:

```
AttributedStmt
└─CaseStmt
  └─…
```

The wrapper sits *outside* the statement it annotates, so matchers and
visitors looking for `CaseStmt` directly under `SwitchStmt`'s compound body
must be prepared to find an `AttributedStmt` there instead. This
wrap-and-delegate pattern (like `ParenExpr` on the expression side) is
exactly what the `Ignore*`-style helpers exist for.

## 11.5 — IfStmt

**`IfStmt`** exposes its parts by role, and half of them are optional:

```cpp
If->getCond();          // Expr*  — always present
If->getThen();          // Stmt*  — always present
If->getElse();          // Stmt*  — null if no else
If->hasInitStorage();   // C++17: if (auto it = m.find(k); it != m.end())
If->getInit();          //   → that init statement
If->hasVarStorage();    // if (Decl cond-var) — declaration as condition
If->getConditionVariable();
If->isConstexpr();      // if constexpr (…)
If->isConsteval();      // C++23: if consteval — and note: NO condition at all
```

Two forms deserve special care from tools:

- `if constexpr` — the false branch of a discarded constexpr-if inside a
  template is *not* instantiated; it may contain code that would be
  ill-formed for this instantiation. Don't assume every branch of every `if`
  you meet in an instantiation was semantically checked.
- `if consteval` (C++23) — dumps as `IfStmt … has_else consteval`
  (verified). It has **no condition expression**: `getCond()` is not a
  meaningful child here, and generic "look at every if's condition" code
  must check `isConsteval()` first or it will misbehave.

## 11.6 — SwitchStmt, CaseStmt, DefaultStmt

**`SwitchStmt`** keeps its cases in two views at once. The body is an
ordinary statement (usually a `CompoundStmt` containing `CaseStmt` /
`DefaultStmt` nodes at arbitrary nesting depth — Duff's device is legal),
and separately the switch threads an intrusive linked list through its
cases:

```cpp
SW->getCond();  SW->getInit();          // C++17 init-statement, like IfStmt
for (SwitchCase *SC = SW->getSwitchCaseList(); SC; SC = SC->getNextSwitchCase())
  ;  // CaseStmt or DefaultStmt, in REVERSE source order
```

The list view is how "missing default" or "case not handled" checks avoid
walking the whole body looking for arbitrarily nested labels. `SwitchCase`
is the common base of exactly two classes:

**`CaseStmt`** — `case 1:`. `getLHS()` is the case value; with Clang's GNU
extension `case 1 ... 5:` there's also `getRHS()`, and the dump marks the
node `gnu_range` (verified — `caseStmt()` matchers meet real ranges in
systems code). Each case owns its *substatement* (`getSubStmt()`), which is
how fallthrough looks in the tree: consecutive labels chain as **nested
substatements**, not siblings —

```
CaseStmt 'case 1:'
└─CaseStmt 'case 2:'
  └─ReturnStmt
```

**`DefaultStmt`** — same shape, no value.

**Quiz.** A style checker wants to flag `switch` statements whose `case`
labels are not all direct children of the switch's `CompoundStmt` (i.e.
labels buried inside nested blocks). Which of the two views of the cases do
you compare against which, and what node from §11.4 can still fool the
"direct child" test even in a perfectly ordinary switch?

> [!hint]- Hint
> One view knows *every* case reachable by the switch, regardless of
> nesting. The other is just tree structure. And remember what `[[likely]]`
> does to a case.

> [!success]- Answer
> Walk `getSwitchCaseList()` for ground truth (every case, however deep),
> and check each node's tree parent (Part 30's `getParents`) against the
> switch's body `CompoundStmt`. An `AttributedStmt` wrapping `[[likely]]
> case …:` inserts itself between the body and the `CaseStmt`, so the
> "direct child" test must look through `AttributedStmt` or it flags
> annotated cases as buried.
