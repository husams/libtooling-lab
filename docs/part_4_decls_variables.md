# Part 4 — Declarations II: Variables, Parameters & Fields

[← Part 3](part_3_decls_infrastructure.md) | [Part 5 — Functions & Methods →](part_5_decls_functions.md)

Eight declaration kinds put a name on storage, and between them they cover
every variable-shaped thing in C++: globals and locals, parameters (written
and hidden), data members (direct, inherited-through-anonymous, and
property-simulated), and the structured-binding pair. Each gets its own
treatment here because tools ask each one different questions.

## 4.1 — VarDecl

`VarDecl` is globals, locals, and statics — everything with variable
semantics that isn't a parameter or member. The daily surface:

```cpp
VD->getType();                    // QualType (Part 25)
VD->hasInit();  VD->getInit();    // initializer Expr*, if any
VD->getStorageClass();            // SC_None, SC_Static, SC_Extern, …
VD->getStorageDuration();         // SD_Automatic / SD_Static / SD_Thread
VD->hasGlobalStorage();           // global or static — outlives its scope
VD->isLocalVarDecl();             // function-local, non-parameter
VD->isStaticLocal();              // function-local static
VD->isStaticDataMember();         // static member — a VarDecl, NOT a FieldDecl
VD->isConstexpr();  VD->isInline();
VD->getInitStyle();               // CInit (= x), CallInit ((x)), ListInit ({x})
VD->evaluateValue();              // fold the initializer to an APValue (or null)
```

The sample's `kPi` answers true to both `hasGlobalStorage()` and
`isConstexpr()`. The three init styles record the difference between
`double x = 1;`, `double x(1);` and `double x{1};` — semantically close,
syntactically distinct, and refactoring tools must preserve which one was
written.

Two classifications trip people up. Static data members are `VarDecl`s
whose `getDeclContext()` is the class — `FieldDecl` is reserved for
*instance* members. And a `VarDecl` inside a template can be a pattern or
an instantiation exactly like functions (Part 7's model);
`VD->getTemplateInstantiationPattern()` disambiguates.

## 4.2 — ParmVarDecl

A subclass of `VarDecl` for function parameters, adding the
parameter-specific dimension:

```cpp
PVD->hasDefaultArg();  PVD->getDefaultArg();   // the Expr, evaluated per call site
PVD->hasUninstantiatedDefaultArg();            // dependent default in a template
PVD->getFunctionScopeIndex();                  // 0-based position
PVD->isParameterPack();                        // Ts... args
```

An unnamed parameter (`void f(int)`) still gets a `ParmVarDecl` — with an
empty name, another reason `getNameAsString()` (which returns `""`) beats
assumptions. Note where default arguments live: on the *parameter*, not on
the call — Part 19's `CXXDefaultArgExpr` is the call-site echo of
`getDefaultArg()`.

One habit worth forming: parameters belong to *each declaration* of a
function separately (redeclaration chains, Part 3 §3.4). The header's
`void f(int x = 3)` and the definition's `void f(int x)` have different
`ParmVarDecl`s — default-argument lookups follow the chain.

## 4.3 — FieldDecl & IndirectFieldDecl

**`FieldDecl`** is non-static data members:

```cpp
FD->getFieldIndex();                    // declaration order, 0-based
FD->isBitField();  FD->getBitWidthValue();
FD->hasInClassInitializer();  FD->getInClassInitializer();
FD->isAnonymousStructOrUnion();
FD->getParent();                        // the RecordDecl
```

The sample's `Point` uses in-class initializers (`double x = 0.0;`), so its
fields report `hasInClassInitializer() == true` — and Part 5 shows how those
initializers get pulled into implicit constructors as `CXXCtorInitializer`
entries with `isWritten() == false`.

**`IndirectFieldDecl`** exists for one C/C++ oddity: anonymous unions and
structs. In

```cpp
struct S { union { int i; float f; }; };
```

`i` is *really* a member of the unnamed union, but `s.i` must work — so
Clang adds an `IndirectFieldDecl` for `i` (and `f`) to `S`, each carrying
the `chain()` of declarations to hop through (`S::<anonymous union>::i`).
Reference-tracking tools must expect `MemberExpr`s that point at either the
`FieldDecl` or the `IndirectFieldDecl` depending on how the member was
reached.

## 4.4 — Structured bindings: DecompositionDecl & BindingDecl

`auto [a, b] = P{1, 2};` produces a small constellation — one hidden
variable plus one node per introduced name. The verified dump:

```
DecompositionDecl … used 'P' cinit
├─CXXConstructExpr …
├─BindingDecl … a 'int'
│ └─DeclRefExpr … 'P' lvalue Decomposition … first_binding 'a'
└─BindingDecl … b 'int'
```

**`DecompositionDecl`** is a `VarDecl` subclass for the unnamed compound
object (note the dump shows no name — `getNameAsString()` returns `""`);
`bindings()` lists its **`BindingDecl`** children. Each `BindingDecl` knows
`getBinding()` — the expression that a use of `a` actually means (for
structs, a member access into the hidden object; for tuple-like types, a
call to `get<I>`) — and `getHoldingVar()` for the tuple case where each
binding gets its own backing variable.

For tools the consequence is symmetrical to §4.3: a `DeclRefExpr` naming
`a` points at the `BindingDecl`, not at any `VarDecl` — "find all
variables" visitors that only handle `VarDecl` silently miss every
structured binding in the codebase.

**Quiz.** After `auto [x, y] = geo::Point{1, 2};`, what does a rename tool
that renames the `FieldDecl` `geo::Point::x` need to know about `x` here?

> [!hint]- Hint
> Which declaration does the *binding* `x` belong to — and is it related to
> the field `x` at all?

> [!success]- Answer
> Nothing needs renaming here — the binding name `x` is an independent
> `BindingDecl`; its spelling coincidentally matches the field. Bindings to
> struct members are positional, not by name. (The rename tool must,
> however, handle `BindingDecl::getBinding()`'s hidden `MemberExpr`, which
> *does* reference the field being renamed.)

## 4.5 — The hidden parameters: ImplicitParamDecl & friends

**`ImplicitParamDecl`** is the node for parameters the compiler invents.
You will not find one in plain C++ function code — they appear when a body
is *outlined* into a compiler-generated function: the captured-region
functions of OpenMP, Objective-C's `self`/`_cmd`, block captures, lambda
conversion thunks. A verified `-fopenmp` dump of `#pragma omp parallel`
shows the pattern:

```
OMPParallelDirective
└─CapturedDecl nothrow
  ├─ImplicitParamDecl implicit .global_tid. 'const int *const __restrict'
  ├─ImplicitParamDecl implicit .bound_tid.  'const int *const __restrict'
  └─ImplicitParamDecl implicit __context   '(unnamed struct …) *const __restrict'
```

`getParameterKind()` says which flavor (`ObjCSelf`, `CapturedContext`,
`ThreadPrivateVar`, …). The practical rule: any visitor that treats "all
`ParmVarDecl`s" as "the user's parameter list" is already correct —
`ImplicitParamDecl` is a *sibling* subclass of `VarDecl`, not a
`ParmVarDecl`, precisely so the two populations never mix.

The last of the eight kinds is Microsoft-specific:

| Kind | Source | Key fact |
|------|--------|----------|
| `MSPropertyDecl` | `__declspec(property(get=GetX, put=PutX)) int x;` under `-fms-extensions` | a fake "member" whose uses become `MSPropertyRefExpr` calls to the getter/setter (Part 24's table) |

Between `VarDecl`, `ParmVarDecl`, `ImplicitParamDecl`, `FieldDecl`,
`IndirectFieldDecl`, `DecompositionDecl`, `BindingDecl`, and
`MSPropertyDecl`, every named-storage declaration in the language now has a
node you can identify on sight.
