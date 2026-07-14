# Part 19 — Expressions: Construction & Initialization

[← Part 18 — Calls](part_18_expr_calls.md) | [Part 20 — Casts →](part_20_expr_casts.md)

Initialization is the most syntactically rich corner of C++ — braces, parens,
designators, defaults, hidden arrays — and the AST gives every mechanism its
own node. This part covers all sixteen of them. The payoff is real: most
"modernize the initialization style" and "find the uninitialized member"
tools live entirely on these nodes.

## 19.1 — CXXConstructExpr: where objects come from

**`CXXConstructExpr`** represents invoking a constructor. It rarely
corresponds to any bracketed syntax — it appears wherever an object comes
into being. The sample's `geo::Circle unit({0.0, 0.0}, 1.0);` dumps as:

```
VarDecl … unit 'geo::Circle' callinit destroyed
`-CXXConstructExpr … 'geo::Circle' 'void (Point, double)'
  ├─InitListExpr … 'Point'
  │ ├─FloatingLiteral 0.0
  │ └─FloatingLiteral 0.0
  └─FloatingLiteral 1.0
```

Key accessors:

```cpp
CCE->getConstructor();       // the CXXConstructorDecl being called
CCE->getNumArgs(); CCE->arguments();
CCE->isElidable();           // copy that may be elided
CCE->isListInitialization(); // was it braced?
CCE->getConstructionKind();  // complete object? base? delegating?
```

Two subclasses refine the syntax story. **`CXXTemporaryObjectExpr`** is the
explicitly-written variant — `Point{1.0, 2.0}` or `std::string("x")` as a
standalone value. And **`CXXInheritedCtorInitExpr`** appears when a
constructor was *inherited*: given

```cpp
struct Base { Base(int); };
struct Derived : Base { using Base::Base; };
Derived d(42);
```

the implicit `Derived(int)` forwards to `Base(int)` through a
`CXXInheritedCtorInitExpr` — no argument re-evaluation, which is exactly what
distinguishes it from a normal delegating construct node (verified: one node
in the dump of the snippet above, inside the synthesized `Derived`
constructor).

## 19.2 — InitListExpr: one node, two forms

Braces produce **`InitListExpr`**, and it leads a double life. Clang keeps
**two forms** of every init list:

```cpp
ILE->isSyntacticForm();         // exactly what was written
ILE->getSemanticForm();         // what it means: reordered to declaration
                                // order, designators resolved, holes filled
ILE->getSyntacticForm();        // cross-link from the semantic node
ILE->getNumInits(); ILE->getInit(i);
ILE->hasArrayFiller();          // "the rest are value-initialized"
```

The *semantic* form is what `-ast-dump` prints and what visitors/matchers
traverse by default. Three filler nodes appear only there:

- **`ImplicitValueInitExpr`** — a member/element you didn't mention,
  value-initialized. `int arr[4] = { [2] = 7 };` dumps as an `InitListExpr`
  whose children are an `array_filler:` note, `ImplicitValueInitExpr`s for
  the skipped slots, and the lone `IntegerLiteral 7` in position 2
  (verified).
- **`NoInitExpr`** — a subobject that *keeps* an earlier initialization
  while a sibling gets overridden (C designated-init update semantics).
- **`DesignatedInitUpdateExpr`** — the override itself:
  `struct Q q = { .in = base, .in.x = 5 };` produces a semantic form where
  `.in` is a `DesignatedInitUpdateExpr` wrapping the copy of `base` plus an
  update list whose untouched field is a `NoInitExpr` (verified: 3 of each
  in the C probe).

Where did the designator go? **`DesignatedInitExpr`** (`.x = 1`, `[2] = 7`)
lives in the **syntactic form only**. This trips up everyone the first time:

```bash
# The default dump shows the SEMANTIC form — no DesignatedInitExpr in sight:
$LLVM/bin/clang -std=c11 -Xclang -ast-dump -fsyntax-only probe.c | grep -c Designated   # 0 hits…

# …yet the node is really there, in the syntactic form, and matchers find it:
$LLVM/bin/clang-query probe.c -- -std=c11
clang-query> match designatedInitExpr()      # 3 matches (verified)
```

A check on designator style must ask for `getSyntacticForm()` (or match
`designatedInitExpr()` directly); a check on *what actually initializes what*
wants the semantic form. Knowing which form you're looking at is the entire
skill of working with init lists.

## 19.3 — The paren-shaped relatives

- **`CXXParenListInitExpr`** — C++20 aggregate initialization *with parens*:
  `Agg a1(1, 2);` where `Agg` has no constructor. Same meaning as braces
  minus narrowing checks and lifetime extension; its own node kind because
  the semantics genuinely differ (verified in the C++20 probe).
- **`ParenListExpr`** — an *unresolved* paren list. It appears where a list
  of expressions was parsed but overload resolution hasn't happened —
  chiefly constructor-initializers inside templates: in
  `template<class T> Pair(T x, T y) : a(x), b(y) {}` the `a(x)` init is a
  `ParenListExpr` while `T` is unknown (verified: three in the template
  probe). At instantiation it becomes a real `CXXConstructExpr` or
  scalar init.
- **`CXXScalarValueInitExpr`** — `int()`, `T()` for scalar `T`: "the zero
  value of this type" (verified). Distinct from a literal `0` — the type is
  computed, which is why generic code produces it constantly.

## 19.4 — std::initializer_list and its hidden array

`std::initializer_list` is a different animal from an aggregate init list.
`ShapeList shapes{&unit};` dumps as:

```
CXXConstructExpr 'void (initializer_list<value_type>)' list
`-CXXStdInitializerListExpr 'initializer_list<geo::Shape *>'
  `-MaterializeTemporaryExpr 'const value_type[1]' xvalue
    `-InitListExpr 'const value_type[1]'
      `-ImplicitCastExpr <DerivedToBase (Shape)>
        `-UnaryOperator '&' → DeclRefExpr 'unit'
```

**`CXXStdInitializerListExpr`** wraps a materialized hidden array — the
`const value_type[1]` temporary the standard says backs the
`initializer_list`. One source line, and you can see the array, its
lifetime-extension node, and even the `Circle*`→`Shape*` base-class
conversion. This is the single best dump in the sample file for calibrating
how much the AST really records.

Two more array-init nodes complete the family, both compiler-generated:
**`ArrayInitLoopExpr`** and **`ArrayInitIndexExpr`**. They appear when an
array must be copied element-by-element — the implicit copy constructor of a
struct with an array member (`WithArr w2 = w1;`, verified: one of each), or
a lambda capturing an array by value. The loop node holds the source as an
`OpaqueValueExpr` (Part 22) and the per-element expression uses the index
node as the loop variable — an entire `for` loop encoded as one expression.

## 19.5 — Defaults: borrowed expressions

Two nodes mark places where the AST inserts *someone else's* expression:

- **`CXXDefaultArgExpr`** — a call site that omitted a defaulted parameter.
  The argument list is padded with these; `getExpr()` returns the default
  expression, which *lives on the `ParmVarDecl`*, not at the call. Its
  source location is the call site, but the text of the expression is at
  the declaration — a classic way for naive rewriters to corrupt code.
- **`CXXDefaultInitExpr`** — a constructor that relies on a member's
  in-class initializer (`double x = 0.0;` in `Point`). It appears in the
  implicit constructors' initializer lists, pointing back at the
  `FieldDecl`'s init.

Both exist so that evaluation order and semantics are explicit in the tree
while the *source of truth* for the expression stays single.

**Quiz.** Your styling tool rewrites every constructor call to braced form.
On `Agg a1(1, 2);` (C++20, `Agg` an aggregate) it finds no
`CXXConstructExpr` at all. What node is the initializer, and name one
semantic change the "equivalent" braced rewrite would introduce.

> [!hint]- Hint
> Aggregates have no constructors to call — and braces are pickier about one
> category of argument conversions.

> [!success]- Answer
> It's a `CXXParenListInitExpr` — aggregate paren-init, no constructor
> involved. Rewriting to `Agg a1{1, 2}` adds *narrowing checks*: if an
> argument were `1.5` or a wider integer, the braced form is ill-formed
> where the paren form silently narrows. (Braces can also extend temporary
> lifetimes for reference members.) Mechanical paren→brace rewrites are not
> semantics-preserving.
