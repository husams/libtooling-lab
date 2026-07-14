# Part 5 — Declarations III: Functions & Methods

[← Part 4](part_4_decls_variables.md) | [Part 6 — Records →](part_6_decls_records.md)

Six declaration kinds form the function family: `FunctionDecl` and its five
C++ children — `CXXMethodDecl`, `CXXConstructorDecl`, `CXXDestructorDecl`,
`CXXConversionDecl`, `CXXDeductionGuideDecl`. Everything a tool ever asks
about callable code starts on `FunctionDecl` and specializes downward, so
this part works the same way.

## 5.1 — FunctionDecl in depth

`FunctionDecl` sits at the bottom of the chain
`NamedDecl → ValueDecl → DeclaratorDecl → FunctionDecl` — a `ValueDecl`
because expressions can refer to functions, a `DeclaratorDecl` because it
was written with declarator syntax and thus carries type-source information
(Part 29). The surface, grouped by question:

```cpp
// Signature
FD->parameters();                 // ArrayRef<ParmVarDecl*>
FD->getNumParams();
FD->getReturnType();              // QualType
FD->isVariadic();                 // trailing ...
FD->getExceptionSpecType();       // EST_BasicNoexcept, EST_None, EST_NoexceptTrue…

// Body
FD->hasBody();  FD->getBody();    // CompoundStmt*, on the defining decl
FD->isThisDeclarationADefinition();
FD->willHaveBody();               // true mid-parse for delayed template bodies

// Specifiers
FD->getStorageClass();            // SC_Static → internal linkage at file scope
FD->isInlined();  FD->isInlineSpecified();
FD->isConstexpr();  FD->isConsteval();  FD->getConstexprKind();
FD->isDeleted();  FD->isDefaulted();  FD->isExplicitlyDefaulted();
FD->isPureVirtual();              // = 0 (renamed from isPure() in LLVM 18)

// Identity
FD->isMain();
FD->isOverloadedOperator();  FD->getOverloadedOperator();   // OO_Plus, OO_Star, …
FD->getLiteralIdentifier();       // for operator""_km
FD->getBuiltinID();               // nonzero for __builtin_* and library builtins
```

Overloading needs no special node: two `FunctionDecl`s with the same name in
the same context simply coexist, and `DeclContext::lookup` returns both.
What connects a *call* to the right overload is the `DeclRefExpr` inside the
call — overload resolution already happened in Sema, and the AST stores its
result (Part 16 picks this up).

Deleted and defaulted functions blur "declaration vs definition":
`= delete` and `= default` both count as definitions
(`isThisDeclarationADefinition()` is true) even though only one of them can
have a body worth visiting. Guard body analysis with `hasBody()`, not with
definition-ness.

**Quiz.** `static double totalArea(const ShapeList &shapes)` — from its
`FunctionDecl`, which calls tell you (a) it has internal linkage, (b) its
parameter count, (c) whether this node owns the body?

> [!hint]- Hint
> One answer is on `FunctionDecl` directly, one is inherited storage-class
> machinery, one came from Part 3's redeclaration chains.

> [!success]- Answer
> (a) `FD->getStorageClass() == SC_Static` (file-scope `static` ⇒ internal
> linkage; the general query is `FD->getLinkageInternal()`), (b)
> `FD->getNumParams()` → 1, (c) `FD->isThisDeclarationADefinition()` /
> `FD->hasBody()` — for `totalArea` there is only one declaration, so this
> node is the definition.

## 5.2 — CXXMethodDecl

`CXXMethodDecl` derives from `FunctionDecl`, so everything above applies;
what it adds is the member-function dimension:

```cpp
MD->getParent();          // the owning CXXRecordDecl
MD->isStatic();  MD->isInstance();
MD->isConst();            // double area() const
MD->getRefQualifier();    // RQ_None / RQ_LValue (&) / RQ_RValue (&&)
MD->isVirtual();          // declared or inherited-virtual
MD->getThisType();        // 'const geo::Shape *' inside a const method
MD->isCopyAssignmentOperator();  MD->isMoveAssignmentOperator();
```

Virtual dispatch relationships are explicit in the AST — no vtable
spelunking required:

```cpp
for (const CXXMethodDecl *Base : MD->overridden_methods()) { … }
MD->size_overridden_methods();   // 0  ⇒ this isn't overriding anything
```

`Circle::area` reports one overridden method (`Shape::area`). Note the
direction: the *derived* method knows its bases, not vice versa — "find all
overriders of X" requires scanning derived classes (or an index), which is
why that query is expensive in every tool that offers it.

The `override` keyword itself is **not** a bit on `CXXMethodDecl` — it's an
attribute node, `OverrideAttr` (Part 10 §10.6): `MD->hasAttr<OverrideAttr>()`.
The distinction powers the classic modernize check:
`size_overridden_methods() > 0 && !hasAttr<OverrideAttr>()` — semantically
overrides, syntactically unmarked.

Lambdas connect here too: a lambda's body is the `operator()` of its closure
class, i.e. a `CXXMethodDecl` with `getParent()->isLambda()` true — which is
how lambda bodies get visited by "all methods" traversals (Part 21 completes
that story).

## 5.3 — CXXConstructorDecl & the initializer list

```cpp
CD->isDefaultConstructor();  CD->isCopyConstructor();  CD->isMoveConstructor();
CD->isConvertingConstructor(/*AllowExplicit=*/false);
CD->isExplicit();            // sample: Shape's ctor IS explicit
CD->isDelegatingConstructor();  CD->getTargetConstructor();
CD->isInheritingConstructor();  // via ConstructorUsingShadowDecl (Part 9 §9.5)
```

The member-initializer list is where the AST is *more complete* than the
source:

```cpp
for (const CXXCtorInitializer *Init : CD->inits()) {
  Init->isBaseInitializer();          // : Shape("circle")
  Init->isMemberInitializer();        // : radius_(radius)
  Init->isDelegatingInitializer();    // : Circle(0,0)
  Init->isInClassMemberInitializer(); // pulled from "double x = 0.0;"
  Init->isWritten();                  // false for compiler-synthesized entries
  Init->getMember();                  // FieldDecl (for member inits)
  Init->getInit();                    // the initializing Expr
}
```

`Circle::Circle(Point center, double radius)` writes three initializers,
but a constructor that omits some members still carries
`CXXCtorInitializer` entries for what actually happens — in-class
initializers and base default constructions — with `isWritten()` telling
you which were typed. Initialization-order linters are a loop over
`inits()` comparing `getMember()->getFieldIndex()` against source order.

## 5.4 — CXXDestructorDecl & CXXConversionDecl

**`CXXDestructorDecl`** adds little API beyond its bases — its identity *is*
the API: at most one per class, virtual-ness inherited, and its existence
suppresses implicit moves (visible in the quiz below).
`RD->getDestructor()` fetches it without iterating.

**`CXXConversionDecl`** is `operator T()`:

```cpp
ConvD->getConversionType();      // the T in "operator T()"
ConvD->isExplicit();             // explicit operator bool()
ConvD->isLambdaToBlockPointerConversion();  // exotic corners exist
```

The sample contains one you never wrote: the lambda in `main` has an
implicit `CXXConversionDecl` to `double (*)(double)` — captureless lambdas
convert to function pointers, and the AST materializes that as a real
conversion function on the closure type.

## 5.5 — Implicit, defaulted, deleted

Dumping `Shape` shows special members you never typed:

```
CXXConstructorDecl … implicit Shape 'void (const Shape &)' inline default noexcept-unevaluated
CXXMethodDecl     … implicit operator= 'Shape &(const Shape &)' inline default
```

Three orthogonal flags to keep straight:

| Query | Meaning | Example |
|-------|---------|---------|
| `isImplicit()` | compiler-declared, zero source text | `Shape`'s copy ctor above |
| `isDefaulted()` | definition is `= default` (written **or** implicit) | `~Shape() = default` |
| `isExplicitlyDefaulted()` | you typed `= default` | `~Shape() = default` |
| `isDeleted()` | `= delete`, or implicitly deleted | move ctor of a class with a const member |

A subtlety visible in the dump: implicit members are only *fully built* if
the program uses them (`noexcept-unevaluated` marks half-built ones).
Visitors don't descend into implicit bodies by default — Part 30's
`shouldVisitImplicitCode()` knob controls exactly this.

**Quiz.** For the sample's `geo::Circle`, is there a move constructor in
the AST — and what does `isImplicit()` return on the *copy* constructor
that appears in its dump?

> [!hint]- Hint
> `Circle` declares no special members at all; `Shape` declares a
> destructor. Think about what an explicit destructor does to implicit
> moves — and who wrote Circle's copy ctor.

> [!success]- Answer
> `Circle`'s copy constructor exists and `isImplicit()` is true — the
> compiler declared it. Whether implicit *move* members appear depends on
> the base: `Shape` has a user-declared destructor, which suppresses
> `Shape`'s implicit moves (copies are used instead), while `Circle` itself
> can still get an implicit move ctor that copy-constructs its base
> subobject. Run
> `clang-query> match cxxConstructorDecl(ofClass(hasName("Circle")))` and
> inspect — the dump settles it, which is the habit this lab is building.

## 5.6 — CXXDeductionGuideDecl

The C++17 newcomer. A deduction guide is function-*shaped* but is never
called — it exists purely to teach class template argument deduction. Given

```cpp
template <class T> struct Box { Box(T); };
Box(const char *) -> Box<int>;      // the guide
Box bx(3);                          // CTAD fires here
```

the verified dump shows the compiler generating guides even before yours:

```
CXXDeductionGuideDecl … implicit <deduction guide for Box> 'auto (T) -> Box<T>'
CXXDeductionGuideDecl … implicit used <deduction guide for Box> 'auto (int) -> Box<int>'
CXXDeductionGuideDecl … <deduction guide for Box> 'auto (const char *) -> Box<int>'
```

Every constructor spawns an implicit guide; explicit ones (yours, third
line) join the set; deduction "calls" the winner conceptually and the used
instantiation is recorded. API-wise: `getDeducedTemplate()` links back to
the `ClassTemplateDecl`, `isExplicit()` distinguishes `explicit` guides, and
its `DeclarationName` is the `CXXDeductionGuideName` kind from Part 3 §3.2
— one more name that would crash `getName()`.

That closes the callable six. Constructors of class templates, method
templates, and everything else "function template shaped" are *not* new
kinds — they're `FunctionTemplateDecl`s wrapping one of these six, which is
exactly where Part 7 picks up.
