# Part 31 — SourceManager & Locations

[← Part 30 — Visitors](part_30_visitors.md) | [Part 32 — Tokens & the Lexer →](part_32_tokens_lexing.md)

Every diagnostic, every rewrite, every "where is this?" goes through the
`SourceManager`. It's also where macros stop being magic and start being two
concrete questions: *where was it written* and *where was it used*. Tools
that get locations wrong emit warnings pointing at the wrong file — this part
is how you avoid being that tool.

## 31.1 — What a SourceLocation really is

A `SourceLocation` is a single 32-bit integer. No file pointer, no line
number — just an offset into the `SourceManager`'s global address space,
which concatenates every buffer the front-end has loaded:

```
sourceloc space:  [ main.cpp ....... ][ <string> ....... ][ macro expansions … ]
                        ▲ FileID 1          ▲ FileID 2         ▲ "expansion" entries
```

That design is why nodes can afford to store several locations each (a
`FunctionDecl` has a start, an end, a name location…) — they're 4 bytes — and
why a location is *useless without the SourceManager* that owns the space.
Decode on demand:

```cpp
SourceManager &SM = Ctx.getSourceManager();
SourceLocation Loc = FD->getLocation();       // the name's location
Loc.printToString(SM)                          // "sample/sample.cpp:57:15"
SM.getFilename(Loc);                           // StringRef
SM.getSpellingLineNumber(Loc);                 // unsigned
SM.getSpellingColumnNumber(Loc);
```

Locations can also be *invalid* (`Loc.isInvalid()`) — compiler-synthesized
entities have no place in source. Check before decoding anything you didn't
produce yourself.

Nodes carry a `getBeginLoc()` / `getEndLoc()` pair (a `SourceRange`) plus
kind-specific ones — for a function, `getLocation()` is the *name*, while
`getBeginLoc()` is the return type or first specifier.

## 31.2 — Decoding locations: file, line, column

Extend the inventory visitor from Part 30 to report positions:

```cpp
bool VisitFunctionDecl(FunctionDecl *FD) {
  SourceManager &SM = Ctx.getSourceManager();
  SourceLocation Loc = FD->getLocation();
  if (!SM.isInMainFile(Loc))
    return true;
  llvm::outs() << FD->getQualifiedNameAsString() << " @ "
               << Loc.printToString(SM) << "\n";
  return true;
}
```

```bash
cmake --build build && ./build/visitor-tool sample/sample.cpp -- -std=c++17
```

Every entry should point into `sample/sample.cpp` with sensible line numbers.
Boring — until a macro is involved.

## 31.3 — Macros: spelling vs expansion

`sample.cpp` computes a circle's area as `kPi * SQUARE(radius_)`, where
`SQUARE` is a macro defined at the top of the file. Inside the expansion,
what's the location of the `*` operator that the macro produced?

There are two honest answers, and the SourceManager keeps both:

- the **spelling location** — where the token's characters were typed: the
  macro *definition*, line 7;
- the **expansion location** — where the macro was *used*: inside
  `Circle::area()`, line 34.

```cpp
if (Loc.isMacroID()) {                        // true inside any expansion
  SM.getSpellingLoc(Loc);                     // → the #define body
  SM.getExpansionLoc(Loc);                    // → the use site
  SM.getFileLoc(Loc);                         // "just give me a real file loc"
}
```

`getFileLoc` is the pragmatic one: it follows the chain until it lands in an
actual file, which is what you want for "where do I point the warning?" —
users care about the use site, not the `#define`. Diagnostics that point
into a macro definition (or worse, into a system header's macro) are how
tools lose users' trust.

**Quiz.** For a token produced by the *argument* of the macro —
`radius_` inside `SQUARE(radius_)` — spelling and expansion location are
**the same place**. Why?

> [!hint]- Hint
> Where were the characters `radius_` actually typed?

> [!success]- Answer
> The argument's characters were typed at the call site, not in the
> `#define`. Only the tokens coming from the macro *body* (`(`, `*`, `)`)
> were spelled at the definition. Clang tracks this per token — which is why
> you must ask per location instead of assuming "it's all macro stuff".

## 31.4 — Ranges and extracting source text

A `SourceRange` is a begin/end pair — but with a subtlety: **the end points
at the *start* of the last token**, not past it. That's called a *token
range*. To slice actual text you convert it to a character range, and the
component that knows token boundaries is the Lexer (Part 32's star, making an
early appearance):

```cpp
#include "clang/Lex/Lexer.h"

CharSourceRange R = CharSourceRange::getTokenRange(FD->getSourceRange());
StringRef Text = Lexer::getSourceText(R, SM, Ctx.getLangOpts());
```

`getSourceText` gives you the exact bytes of the declaration as written —
whitespace, comments and all. It's the backbone of every "show the offending
code" feature, and the read half of source rewriting.

Try it: print each function's first line of source next to its name
(`Text.take_until([](char c){ return c == '\n'; })`).

## 31.5 — Filtering: main file, system headers, presumed locations

You've been using `SM.isInMainFile(Loc)` since Part 30. Its siblings complete
the toolbox for deciding *which* code is yours to analyze:

```cpp
SM.isInSystemHeader(Loc);        // <vector>, SDK headers … per -isystem/-internal paths
SM.isWrittenInMainFile(Loc);     // stricter: spelling loc is in the main file
SM.getMainFileID();              // the FileID of the TU's primary file
```

Real tools default to "warn only in the main file, never in system headers" —
exactly what clang-tidy does — because you can't fix `<vector>` and your
users can't either.

Last curiosity: **presumed locations**. The preprocessor's `#line` directive
(and code generators that emit it) can claim that the following code "really"
lives at some other file/line. `SM.getPresumedLoc(Loc)` honors those lies,
which is what compilers use for diagnostics in generated code
(`lex.yy.c` → `scanner.l`). The spelling/expansion machinery always tells
the physical truth; presumed locations tell the polite fiction.

**Quiz.** Your linter warns on a `DeclRefExpr` whose location satisfies
`isMacroID()`. The macro is defined in a third-party header but *used* in the
main file. `isInMainFile(Loc)` returns… what, and which call fixes the check?

> [!success]- Answer
> For a token from the macro body, the raw `Loc` is an expansion-space
> location; `isInMainFile` resolves it via the expansion side, so it returns
> *true* (the use site is in the main file) — usually what you want. If you
> instead need "was this *typed* in the main file", that's
> `isWrittenInMainFile(SM.getSpellingLoc(Loc))`, which returns false here.
> Deciding which of the two questions you're asking *is* the fix.
