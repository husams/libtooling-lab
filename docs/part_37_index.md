# Part 37 — The Index Library: USRs & Symbol Occurrences

[← Part 36 — The Preprocessor](part_36_preprocessor_includes.md) | [Part 38 — Rewriting & Edits →](part_38_rewrite_edit.md)

Visitors and matchers hand you `Decl*` and `Stmt*` pointers — perfect while
one AST is alive, useless the moment it isn't. A pointer is identity *within
one parse*; the same `geo::Shape` compiled in two translation units is two
unrelated addresses. Every tool that reasons across files — "find all
references", a call hierarchy, `clangd`'s background index — needs identity
that *survives* the parse. That is what `clang/Index` provides: a stable
string name for every entity (the USR), and a normalized stream of symbol
*occurrences* tagged with the roles each one plays. It's a third door onto
the same AST, built for cross-TU work.

The lab's fourth skeleton, `skeletons/index-tool/`, is wired for this part.
Build it exactly like the others:

```bash
cd skeletons/index-tool
cmake -G Ninja -B build -DCMAKE_PREFIX_PATH=$(brew --prefix llvm) \
  -DCMAKE_CXX_COMPILER=$(brew --prefix llvm)/bin/clang++ .
cmake --build build
./build/index-tool sample/sample.cpp -- -std=c++17
```

## 37.1 — USRs: a name that outlives the parse

A **USR** — Unified Symbol Resolution — is a compiler-independent string that
identifies an entity by *what it is and where it lives*, not by its address.
Generate one from any `Decl`:

```cpp
#include "clang/Index/USRGeneration.h"

llvm::SmallString<128> USR;
clang::index::generateUSRForDecl(D, USR);   // false on success (it can decline)
```

For the sample's `geo::Shape::area` override in `Circle`, that yields:

```
c:@N@geo@S@Circle@F@area#1
```

Read it as a path: `@N@geo` namespace, `@S@Circle` struct/class, `@F@area`
function, `#1` a mangling of the signature. Parse the *same header* in a
different TU and you get the *same* string — which is the whole point.
Compute the USR for `Shape::area` and `Circle::area` and they differ; compute
it for a declaration and its definition and they match. Sibling APIs cover the
other name kinds: `generateUSRForType`, `generateUSRForMacro`,
`generateUSRForModule`.

One wrinkle visible in the sample's output: local statics and file-local
entities get the *filename* baked in —
`c:sample.cpp@N@geo@kPi` for the `static`-linkage `constexpr kPi`. That's
deliberate: an internal-linkage symbol is a *different* entity in every TU, so
its USR must not collide across files.

## 37.2 — Symbol info and roles

Beyond identity, the library classifies each symbol and records what it's
*doing* at a given spot. Two pieces:

```cpp
#include "clang/Index/IndexSymbol.h"

SymbolInfo Info = getSymbolInfo(D);          // Info.Kind, Info.Lang, ...
getSymbolKindString(Info.Kind);              // "class", "instance-method", ...
printSymbolRoles(Roles, llvm::outs());       // "Def,Dyn,RelChild,RelOver"
```

`SymbolKind` is a normalized taxonomy — `Class`, `Struct`, `Field`,
	`InstanceMethod`, `Constructor`, `Namespace`, `TypeAlias`, … — coarser than
Clang's 91 `Decl` kinds, because it's meant to be stable across languages and
Clang versions (it's what an IDE's symbol filter offers).

A **`SymbolRoleSet`** is a bitset over `SymbolRole` describing *this
occurrence*, not the symbol. The ones you'll actually read:

| Role | Meaning |
|------|---------|
| `Declaration` / `Definition` | this spelling declares vs defines the entity |
| `Reference` | a use, not a declaration |
| `Read` / `Write` | how a variable/field is touched here |
| `Call` | this reference is a call site |
| `Dynamic` | the call/method is virtual (dynamic dispatch) |
| `RelationChild` / `RelationBase` / `RelationOverride` … | structural relations to another symbol |

The same name produces *many* occurrences with different role sets — which is
exactly the data a cross-reference database is built from.

## 37.3 — The occurrence stream

You don't walk the tree yourself; you subscribe. Subclass
`IndexDataConsumer` and override the hook that fires once per symbol use:

```cpp
#include "clang/Index/IndexDataConsumer.h"

class IndexConsumer : public index::IndexDataConsumer {
public:
  bool handleDeclOccurrence(const Decl *D, SymbolRoleSet Roles,
                            ArrayRef<SymbolRelation>, SourceLocation Loc,
                            ASTNodeInfo) override {
    // D is the canonical decl; Roles/Loc describe THIS spelling.
    return true;   // false aborts the whole index run
  }
};
```

There are sibling hooks — `handleMacroOccurrence`, `handleModuleOccurrence` —
so a single consumer sees AST *and* preprocessor symbols through one
interface. The Index library drives it with a purpose-built `FrontendAction`:

```cpp
#include "clang/Index/IndexingAction.h"

auto Consumer = std::make_shared<IndexConsumer>();
index::IndexingOptions Opts;                 // SystemSymbolFilter, IndexFunctionLocals…
// createIndexingAction returns a FrontendAction; wrap it in a factory for ClangTool:
return index::createIndexingAction(Consumer, Opts);
```

`ClangTool::run` wants a *factory* (one fresh action per TU), so the skeleton
bridges the two with a tiny `FrontendActionFactory` whose `create()` calls
`createIndexingAction`. `IndexingOptions` is where you widen the net —
`IndexFunctionLocals` to see locals, `SystemSymbolFilter` to include or
exclude the standard-library symbols the headers drag in.

**Quiz.** In `handleDeclOccurrence`, the same `const Decl *D` pointer arrives
for a declaration, its definition, and every reference — but you're told to
key a cross-TU database on the *USR*, not on `D`. Both uniquely identify the
symbol within this run, so why is the pointer the wrong key?

> [!hint]- Hint
> Think about the tool that indexes 2,000 files. How many of those `Decl*`
> values still mean anything when file #2 is being parsed?

> [!success]- Answer
> `D` is only valid inside the `ASTContext` that produced it. A real indexer
> parses each TU, drains its occurrences, then *destroys* that AST before the
> next — so the pointer is dangling by the time you'd cross-reference it, and
> the "same" entity in another TU has a different address entirely. The USR is
> a value you can serialize to disk and match across runs; the pointer can't
> leave the parse it was born in. That gap between per-parse identity and
> durable identity is the entire reason `clang/Index` exists.

## 37.4 — Hands-on: reading the cross-reference stream

Run the skeleton as-is. Each line is one occurrence — location, kind, role
set, USR:

```
sample.cpp:19:7   class            [Def,RelChild]                c:@N@geo@S@Shape
sample.cpp:23:18  instance-method  [Decl,Dyn,RelChild]           c:@N@geo@S@Shape@F@area#1
sample.cpp:34:10  instance-method  [Def,Dyn,RelChild,RelOver]    c:@N@geo@S@Circle@F@area#1
sample.cpp:34:41  variable         [Ref,Read,RelCont]            c:sample.cpp@N@geo@kPi
sample.cpp:34:54  field            [Ref,Read,RelCont]            c:@N@geo@S@Circle@FI@radius_
```

Every lesson from this part is legible in those five lines: `Shape::area` is
`Decl,Dyn` (a declaration of a virtual), `Circle::area` is `Def,Dyn,RelOver`
(a definition that *overrides* — the `RelOver` relation points back at the
base method), and inside it `kPi` and `radius_` are `Ref,Read` uses. Group
the whole stream by USR and you have built the core index that powers
*find-all-references* and *go-to-definition*: collect every occurrence whose
USR equals the symbol under the cursor, split declarations/definitions from
references by role, and you're done.

Two natural extensions turn the skeleton into real tools. Keep only
occurrences whose `Roles & (unsigned)SymbolRole::Call` is set and record the
`RelationCall` target, and the output is a **call graph** keyed by USR — the
same data Part 39's `CallGraph` builds from the AST, but serializable across
TUs. Filter to `SymbolRole::Dynamic` calls and you have located every
virtual-dispatch site in the program. This is the exact machinery behind
`clangd`'s background index and the `clang -index-*` toolchain; you're now
holding its raw feed.
