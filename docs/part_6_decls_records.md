# Part 6 — Declarations IV: Records

[← Part 5](part_5_decls_functions.md) | [Part 7 — Templates I →](part_7_decls_templates_functions.md)

A C++ class is the densest declaration in the language, and `CXXRecordDecl`
is correspondingly the densest node in the AST: bases, members, access
control, friends, layout — plus a crowd of implicit declarations the
compiler adds on its own. Five declaration kinds live here: `RecordDecl`,
`CXXRecordDecl`, `AccessSpecDecl`, `FriendDecl`, and `FriendTemplateDecl`,
worked through on `geo::Shape` and `geo::Circle`.

## 6.1 — RecordDecl & CXXRecordDecl

The inheritance path is `TypeDecl → TagDecl → RecordDecl → CXXRecordDecl`.
`TagDecl` contributes the "tag kind" — one node class serves `struct`,
`class`, and `union` alike. **`RecordDecl`** by itself is what you get in
pure C (and for some compiler-internal records); in a C++ TU virtually
every record is the subclass. Shared surface, then the C++ additions:

```cpp
// TagDecl / RecordDecl level
RD->isStruct();  RD->isClass();  RD->isUnion();     // keyword used
RD->isThisDeclarationADefinition();                 // fwd decl vs the { … } one
RD->hasDefinition();  RD->getDefinition();
RD->isCompleteDefinition();
RD->fields();                                       // FieldDecl range (Part 4)
RD->isAnonymousStructOrUnion();

// CXXRecordDecl level
RD->isPolymorphic();     // has/inherits a virtual function
RD->isAbstract();        // has/inherits an unoverridden pure virtual
RD->isAggregate();  RD->isPOD();  RD->isTriviallyCopyable();  RD->isEmpty();
RD->isLocalClass();  RD->isLambda();                // lambdas ARE CXXRecordDecls (Part 21)
RD->methods();  RD->ctors();  RD->getDestructor();
RD->hasUserDeclaredConstructor();  RD->needsImplicitDefaultConstructor();
```

⚠️ Almost every interesting query — bases, methods, polymorphism — is only
valid on a **definition**. On a forward declaration (`class Circle;`) they
assert or lie. The defensive incantation at the top of any class analysis:

```cpp
if (!RD->hasDefinition()) return true;   // or: RD = RD->getDefinition()
```

Dump `geo::Shape` and the first child is a surprise:

```
CXXRecordDecl … class Shape definition
├─CXXRecordDecl … implicit referenced class Shape     ← ?!
├─CXXConstructorDecl … Shape 'void (std::string)'
…
```

That implicit inner `Shape` is the **injected-class-name** — the C++ rule
that inside `Shape`, the name `Shape` refers to the class itself. Clang
materializes the rule as an actual declaration (`isInjectedClassName()`
identifies it). It's the first of many implicit members you must expect —
and usually skip with `isImplicit()` — when iterating a class.

## 6.2 — Bases

Base classes live on the definition as a sequence of `CXXBaseSpecifier` —
not declarations themselves, but records of the inheritance clause:

```cpp
for (const CXXBaseSpecifier &B : RD->bases()) {
  B.getType();            // QualType of the base — geo::Shape
  B.getAccessSpecifier(); // AS_public / AS_protected / AS_private
  B.isVirtual();
  B.isPackExpansion();    // struct overloaded : Ts...
}
RD->getNumBases();  RD->vbases();          // virtual bases, flattened
```

Getting from the specifier to the base's own `CXXRecordDecl` goes through
the type system: `B.getType()->getAsCXXRecordDecl()`. For hierarchy
questions you rarely walk this yourself:

```cpp
Circle->isDerivedFrom(Shape);        // transitive, ignores access
Circle->isVirtuallyDerivedFrom(X);
Circle->forallBases([](const CXXRecordDecl *Base) { …; return true; });
```

`isDerivedFrom` is the workhorse behind every "does this class inherit from
the framework base?" check — matchers' `isDerivedFrom("geo::Shape")`
(Part 33) is a thin wrapper over it.

## 6.3 — Access & AccessSpecDecl

Access control is stored twice, by design. Every member answers
`D->getAccess()` (`AS_public`, `AS_protected`, `AS_private` — `AS_none`
outside classes). And the *labels themselves* are nodes: an
**`AccessSpecDecl`** appears in declaration order wherever `public:` /
`private:` was written:

```
CXXRecordDecl … class Shape definition
├─AccessSpecDecl … public
├─CXXConstructorDecl …
├─AccessSpecDecl … private
└─FieldDecl … name_
```

That's how a rewriting tool knows where the sections are — "insert the new
method after the last member of the `public:` section" is an iteration over
`decls()` watching for `AccessSpecDecl` boundaries. It's also the only
declaration kind that is pure syntax: no name, no type, no semantics beyond
setting the default for what follows. `Point` (a `struct`) has no
`AccessSpecDecl` at all, and its members default to `AS_public` — the
default flips, the node only appears when typed.

## 6.4 — Friends: FriendDecl & FriendTemplateDecl

Friendship is a declaration too. **`FriendDecl`** holds either a type
(`friend class X;` → `getFriendType()`) or a declaration
(`friend void f();` → `getFriendDecl()`):

```cpp
for (const FriendDecl *FR : RD->friends()) {
  if (const TypeSourceInfo *TSI = FR->getFriendType())      // friend class X;
    …
  else if (const NamedDecl *D = FR->getFriendDecl())        // friend void f();
    …
}
```

Friends live in the class's declaration list but do **not** make the
friended entity a member — its `getDeclContext()` is still the enclosing
namespace, one more case where semantic and lexical context (Part 3 §3.3)
part ways. Template friends (`template <class U> friend class Wrapper;`)
still arrive as `FriendDecl`s whose `getFriendDecl()` is a
`ClassTemplateDecl`; the separate **`FriendTemplateDecl`** kind exists in
the taxonomy for friend templates of *templates* with their own parameter
lists, but Clang currently builds `FriendDecl`s in practice — recognize the
name, expect to see it rarely (the class comment in `DeclFriend.h` says as
much).

**Quiz.** `friend void helper(Circle &);` appears inside `class Circle`.
Your "find all functions in namespace geo" visitor — does it see `helper`,
and what is `helper`'s `getDeclContext()` vs `getLexicalDeclContext()`?

> [!hint]- Hint
> Friends declare real entities — into which scope?

> [!success]- Answer
> Yes: the friend declaration *declares* `geo::helper` (first declaration
> wins if none existed). Its semantic `getDeclContext()` is namespace `geo`
> — that's where the entity lives — while `getLexicalDeclContext()` is
> `Circle`, where the text sits. A visitor reaches it while traversing the
> class; a `DeclContext::decls()` walk of the namespace won't show it until
> a namespace-scope redeclaration appears. (Argument-dependent lookup still
> finds it — one of C++'s deeper rabbit holes.)

## 6.5 — Record layout

The AST can answer ABI-level questions — sizes, alignments, offsets — via
`ASTContext`, which consults the target:

```cpp
const ASTRecordLayout &L = Ctx.getASTRecordLayout(RD);
L.getSize();                 // CharUnits — whole-object sizeof
L.getAlignment();
L.getFieldOffset(F->getFieldIndex());   // ⚠️ in BITS, not bytes
Ctx.getTypeSizeInChars(QT);  // sizeof for any complete type
```

Clang will print the whole computation for you:

```bash
$LLVM/bin/clang++ -std=c++17 -fsyntax-only -Xclang -fdump-record-layouts \
  sample/sample.cpp
```

For `geo::Circle` on arm64 this prints (abridged):

```
 0 | class geo::Circle
 0 |   class geo::Shape (primary base)
 0 |     (Shape vtable pointer)
 8 |     std::string name_
32 |   struct geo::Point center_
48 |   double radius_
   | [sizeof=56, align=8]
```

Everything about the layout algorithm is on display: the vtable pointer
materialized at offset 0 because `Shape` is polymorphic, the base subobject
packed first, `std::string` occupying 24 bytes, then `Circle`'s own fields.
Tools that check struct packing, cache-line placement, or serialization
compatibility are just this API plus opinions — and note the classic unit
trap: `getFieldOffset` returns **bits** (`radius_` → 384, not 48).
