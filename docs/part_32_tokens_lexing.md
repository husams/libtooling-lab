# Part 32 — Tokens & the Lexer

[← Part 31 — SourceManager](part_31_source_manager.md) | [Part 33 — AST Matchers →](part_33_ast_matchers.md)

The AST is a heavily processed view of the program: comments are gone,
whitespace is gone, macros are expanded, punctuation has become structure.
Sometimes a tool needs the layer *below* — the raw token stream. Formatters
live there entirely (`clang-format` never builds an AST), and even
AST-centric tools drop down to the lexer for text-level questions.

## 32.1 — Where tokens sit in the pipeline

```
bytes ──lexer──► tokens ──preprocessor──► tokens' ──parser/sema──► AST
"kPi*x"          kPi * x                  (macros expanded,        BinaryOperator
                                           directives executed)      ├─ DeclRefExpr kPi
                                                                     └─ DeclRefExpr x
```

Two token streams exist, and the difference matters:

- **raw tokens** — exactly what's in the file: `#`, `define`, comments (if
  asked), the *unexpanded* macro names;
- **preprocessed tokens** — what the parser sees: macros expanded, skipped
  `#if 0` regions absent.

The AST is built from the second stream, which is precisely why Part 31's
spelling/expansion split exists — expansion locations are the bridge back.

## 32.2 — The Token class

A `Token` is small and value-like:

```cpp
Token Tok;
Tok.getKind();           // tok::identifier, tok::l_paren, tok::kw_return, …
Tok.getLocation();       // SourceLocation of its first character
Tok.getLength();
Tok.is(tok::identifier); Tok.isNot(tok::eof); Tok.isOneOf(tok::plus, tok::minus);
tok::getTokenName(Tok.getKind());          // "identifier", "l_paren", …
Lexer::getSpelling(Tok, SM, LangOpts);     // its text, as std::string
```

Kinds are a closed enum (`clang/Basic/TokenKinds.def`): keywords are their
own kinds (`kw_virtual`), every punctuator has one (`arrow`, `coloncolon`),
identifiers are one kind whose *spelling* differentiates them. Note what a
raw lexer can't know: whether `clamp` is a template name or a variable —
that's the parser's job. Tokens are syntax without meaning.

## 32.3 — Raw lexing a file

You can lex any buffer the SourceManager holds, independent of the AST.
Add a token inventory to the visitor tool's consumer (or a standalone
function called from `HandleTranslationUnit`):

```cpp
#include "clang/Lex/Lexer.h"

void lexMainFile(ASTContext &Ctx) {
  SourceManager &SM = Ctx.getSourceManager();
  FileID FID = SM.getMainFileID();
  llvm::MemoryBufferRef Buf = SM.getBufferOrFake(FID);

  Lexer Lex(FID, Buf, SM, Ctx.getLangOpts());
  Lex.SetCommentRetentionState(true);          // comments become tokens too

  Token Tok;
  unsigned Count = 0, Comments = 0;
  while (!Lex.LexFromRawLexer(Tok)) {          // returns true at eof
    ++Count;
    if (Tok.is(tok::comment))
      ++Comments;
  }
  llvm::outs() << Count << " raw tokens, " << Comments << " comments\n";
}
```

Run it on the sample: a few hundred tokens, and the file's comments are
countable — something the AST could never tell you. Upgrade it: print the
first 40 tokens as `kind "spelling" @ line`, and you've written the front
half of a syntax highlighter.

A raw lexer starts in "I know nothing" mode: it will happily hand you the
`#`, `include`, `<`, `string`, `>` of a directive as five tokens. It doesn't
execute directives, expand macros, or even combine `>>` correctly in template
contexts. That's the deal — raw speed and raw truth.

**Quiz.** Why does the AST throw comments away, and what are the *two*
official escape hatches for a tool that needs "the comment attached to this
declaration"?

> [!success]- Answer
> Comments don't affect semantics, and keeping every token would bloat a tree
> that's already the biggest allocation in a compile. Escape hatches: (1)
> doc-comments — Clang *does* retain `///` and `/** */` when attached to
> declarations: `Ctx.getCommentForDecl(D, nullptr)` (or compile with
> `-fparse-all-comments` to attach plain ones too); (2) re-lex the range near
> the declaration with a raw lexer, as above. clang-tidy uses both.

## 32.4 — The Lexer's static helpers

For an AST-based tool, the most common lexer use is not a loop — it's the
static utilities that answer token-boundary questions about locations you
already have:

```cpp
// Where does the token starting at Loc end?
SourceLocation End = Lexer::getLocForEndOfToken(Loc, /*Offset=*/0, SM, LangOpts);

// The exact text of any range (Part 31's workhorse):
StringRef Text = Lexer::getSourceText(CharSourceRange::getTokenRange(R), SM, LangOpts);

// The next token after a location — "is there a semicolon after this?":
std::optional<Token> Next = Lexer::findNextToken(Loc, SM, LangOpts);
if (Next && Next->is(tok::semi)) { … }

// How long is the token at Loc?
unsigned Len = Lexer::MeasureTokenLength(Loc, SM, LangOpts);
```

These matter because AST ranges are *token* ranges (end = start of last
token). Anything that inserts text "after" a node — a fix-it appending
`override`, a rewriter adding `;` — needs `getLocForEndOfToken` to find the
true end. This family of calls is the connective tissue between the AST
layer and the text layer.

## 32.5 — Hands-on: a token inventory

Consolidate the part into the tool: add a mode that, for the main file,
reports

1. total token count, comment count (raw lexing, §32.3),
2. the top-5 most frequent identifiers (a `llvm::StringMap<unsigned>` of
   spellings),
3. for each function found by your Part 30 visitor, the token that follows
   its body — using `findNextToken(FD->getBody()->getEndLoc(), …)` (guard
   with `hasBody()`).

Everything needed is on this page and Part 31. When it runs, compare (1)
against `grep -c` intuition and make the numbers make sense.

**Quiz.** Your raw-token count for `sample.cpp` is ~370, but the
preprocessor-visible token count for the TU is in the hundreds of thousands.
Where's the factor of a thousand coming from?

> [!success]- Answer
> `#include <string>` and `<vector>`: the raw lexer sees five tokens of
> directive; the preprocessor replaces them with the *entire contents* of the
> headers (and their transitive includes). Raw = this file's bytes;
> preprocessed = the translation unit.
