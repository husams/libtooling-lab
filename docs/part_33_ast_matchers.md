# Part 33 — AST Matchers

[← Part 32 — Tokens & the Lexer](part_32_tokens_lexing.md) | [Part 34 — Command-Line APIs →](part_34_command_line.md)

Most tool logic is not "walk everything" — it's "find nodes shaped like
*this*". AST matchers are a declarative DSL for exactly that: you describe
the shape, Clang finds the nodes. This part moves to the second skeleton,
`skeletons/matcher-tool`, and to the fastest feedback loop in all of Clang
tooling: `clang-query`.

## 33.1 — Matchers vs visitors

The same check, both ways — "virtual methods named `area`":

```cpp
// Visitor: imperative
bool VisitCXXMethodDecl(CXXMethodDecl *MD) {
  if (MD->isVirtual() && MD->getNameAsString() == "area") report(MD);
  return true;
}

// Matcher: declarative
cxxMethodDecl(isVirtual(), hasName("area")).bind("m")
```

Rules of thumb: matchers win when the pattern is *structural* (nesting,
properties, relationships) — they read like the sentence describing the
check, and clang-tidy is essentially a matcher collection. Visitors win when
you need *state across nodes* (counting, pairing declarations with uses) or
exhaustive inventories. Real tools mix both: match to find candidates, then
navigate with node APIs.

## 33.2 — Prototyping in clang-query

`clang-query` evaluates matchers interactively against a live AST — always
prototype here before compiling anything:

```bash
$LLVM/bin/clang-query skeletons/visitor-tool/sample/sample.cpp -- -std=c++17
```

```
clang-query> match cxxRecordDecl(isExpansionInMainFile())
…
clang-query> set output dump
clang-query> match cxxMethodDecl(isOverride())
clang-query> set output diag
clang-query> match functionDecl(parameterCountIs(3))
```

`set output dump` shows the full AST subtree per match — this is the
interactive `-ast-dump` you met in Part 2. Iterate on the matcher
until the match set is exactly right, then paste it into C++ unchanged.

## 33.3 — The matcher grammar

Every matcher expression is built from three kinds of pieces:

- **node matchers** — name a node kind, and are the only things that can be
  outermost: `functionDecl()`, `cxxRecordDecl()`, `callExpr()`, `stmt()`,
  `qualType()`. Spelling rule: lowercase-camel of the class name minus
  suffixes (`CXXRecordDecl` → `cxxRecordDecl`).
- **narrowing matchers** — filter the current node by a property:
  `hasName("area")`, `isVirtual()`, `parameterCountIs(2)`, `isConst()`,
  `isExpansionInMainFile()`.
- **traversal matchers** — jump to a related node and apply a sub-matcher
  there: `hasParameter(0, …)`, `hasReturnType(…)`, `hasBody(…)`,
  `hasArgument(0, …)`, `hasAncestor(…)`, `hasDescendant(…)`.

Composition is just nesting, and it reads outside-in:

```cpp
cxxMethodDecl(                       // a method…
  isVirtual(),                       // …that is virtual
  ofClass(isDerivedFrom("geo::Shape")),  // …of a class derived from Shape
  hasBody(compoundStmt())            // …with a body
)
```

The full vocabulary (hundreds of matchers) lives in the
[AST Matcher Reference](https://clang.llvm.org/docs/LibASTMatchersReference.html);
nobody memorizes it — you learn the grammar and look up the words.

## 33.4 — MatchFinder, callbacks, and binding

Build the matcher skeleton (same drill as Part 1):

```bash
cd skeletons/matcher-tool
cmake -G Ninja -B build -DCMAKE_PREFIX_PATH=$LLVM -DCMAKE_CXX_COMPILER=$LLVM/bin/clang++ .
cmake --build build
./build/matcher-tool sample/sample.cpp -- -std=c++17
```

Its `main.cpp` shows the whole compiled-matcher pattern in ~20 lines:

```cpp
MatchFinder Finder;
Callback CB;
Finder.addMatcher(functionDecl(isExpansionInMainFile()).bind("fn"), &CB);
Tool.run(newFrontendActionFactory(&Finder).get());
```

`.bind("fn")` labels the node the matcher lands on; the callback retrieves it
by label and static type:

```cpp
void run(const MatchFinder::MatchResult &Result) override {
  if (const auto *FD = Result.Nodes.getNodeAs<FunctionDecl>("fn"))
    llvm::outs() << FD->getQualifiedNameAsString() << "\n";
}
```

You can `.bind()` at *several* points in one matcher — the match brings all
of them, which is how a check grabs both "the bad call" and "the argument to
point the fix-it at". `Result.Context` and `Result.SourceManager` hand you
everything Parts 2–31 taught, so a callback body is ordinary node-API code.

## 33.5 — Composing matchers

The logical operators finish the grammar: `allOf(…)` (implicit in every node
matcher's argument list), `anyOf(…)`, `unless(…)` — plus the two quantifiers
whose difference bites everyone once:

- `has(…)` — a **direct child** matches
- `hasDescendant(…)` — **anything below** matches
- `forEachDescendant(…)` — like `hasDescendant`, but produces **one match
  per descendant** instead of one per ancestor

```
match callExpr(hasDescendant(declRefExpr()))      one match per call
match callExpr(forEachDescendant(declRefExpr()))  one match per name inside each call
```

Try both in clang-query on `doubled(totalArea(shapes))` and compare counts —
that's the cleanest demonstration of the distinction you'll ever get.

**Quiz.** `functionDecl(hasName("clamp"))` matches the template in
`sample.cpp` — but write a matcher that fires once per *instantiation*.

> [!hint]- Hint
> Instantiations are `FunctionDecl`s too; there's a narrowing matcher named
> after exactly what they are.

> [!success]- Answer
> `functionDecl(hasName("clamp"), isTemplateInstantiation())` — two matches
> (`clamp<int>`, and the `double` one if you add a call). The bare template
> pattern is excluded by the narrowing matcher; by default matchers *do* see
> instantiations, unlike visitors.

## 33.6 — Traversal modes and implicit nodes

The invisible nodes of Parts 20–22 come back to haunt matchers. Naively,
`varDecl(hasInitializer(integerLiteral()))` fails on `double y = 1;` — the
initializer is an `ImplicitCastExpr` around the literal. Two solutions:

```cpp
// 1. Old style: peel explicitly
varDecl(hasInitializer(ignoringImpCasts(integerLiteral())))

// 2. Modern style: change the traversal mode
traverse(TK_IgnoreUnlessSpelledInSource,
         varDecl(hasInitializer(integerLiteral())))
```

`TK_IgnoreUnlessSpelledInSource` makes the whole sub-matcher see the AST *as
written* — implicit casts, implicit constructors, materialized temporaries
all become invisible. In clang-query: `set traversal
IgnoreUnlessSpelledInSource`. It's the right default for style checks;
stay in the default `TK_AsIs` when the implicit nodes *are* the point
(finding hidden conversions, lifetime issues).

## 33.7 — Exercises

Prototype each in clang-query, then add it to `matcher-tool` with a bound
node and a useful report line (name + location — Part 31 skills):

1. Functions with more than two parameters.
   > [!hint]- Hint
   > `unless(parameterCountIs(…))` won't do it — count upward: there's no
   > `parameterCountAtLeast`, but `hasParameter(2, anything())` means "a
   > parameter #3 exists".
2. Virtual methods that override without spelling `override`
   (`isOverride()` but… look for a narrowing matcher about attributes — or
   invert: `cxxMethodDecl(isOverride(), unless(hasAttr(clang::attr::Override)))`).
3. Calls to `totalArea` — and bind *both* the call and its argument
   (`callee(…)`, `hasArgument(0, …)`, two `.bind()`s).
4. Classes that have a virtual method but no virtual destructor —
   `cxxRecordDecl(has(cxxMethodDecl(isVirtual())), unless(has(cxxDestructorDecl(isVirtual()))))`;
   explain why `has` vs `hasDescendant` matters here, then check it against
   `geo::Shape` (should not fire) and a class you add that should.
