# Part 15 — Expressions: Literals

[← Part 14 — The Expr Base](part_14_expr_base.md) | [Part 16 — Names & Members →](part_16_expr_names_members.md)

Literals are where source text becomes semantic value, and Clang gives every
literal form its own node class carrying the *exact* value — in
arbitrary-precision types, because the host running your tool and the target
being compiled may not agree on what fits. This part covers all twelve
literal kinds.

## 15.1 — Numbers: IntegerLiteral and FloatingLiteral

```cpp
IL->getValue();          // llvm::APInt — arbitrary precision
FL->getValue();          // llvm::APFloat
FL->getValueAsApproximateDouble();   // convenience, lossy by definition
```

Values come back as `APInt`/`APFloat` because the literal's value may not
fit any host integer and its meaning depends on the target
(`getValue().getZExtValue()` only after you've checked `getBitWidth()`).
The literal's *type* (`getType()`) already encodes the suffix: `42` is
`int`, `42u` is `unsigned int`, `42.0f` is `float`.

Note what a literal node is *not*: `-1` is **not** a literal — it's a
`UnaryOperator UO_Minus` around `IntegerLiteral 1`. Sign lives in the
operator. Every "check the constant" tool eventually learns this the hard
way; check for the pattern, not just the node.

Two rarer numeric kinds complete the set:

- **`FixedPointLiteral`** — `0.5r`, `2.5k` under `-ffixed-point`
  (Embedded-C fixed-point types; C only). You will likely never meet one
  outside DSP code, but the node exists and dumps like an integer literal
  with a scale.
- **`ImaginaryLiteral`** — GNU extension `2.0i` / `3.0fi` for `_Complex`
  arithmetic; wraps the underlying `FloatingLiteral` or `IntegerLiteral`
  (`getSubExpr()`).

## 15.2 — Characters and strings

**`CharacterLiteral`** stores `getValue()` as `unsigned` plus a kind that
records the prefix — and the kind changes the node's *type*:

| Prefix | `getKind()` | Type |
|--------|-------------|------|
| `'x'` | `CharacterLiteralKind::Ascii` | `char` (`int` in C!) |
| `L'x'` | `Wide` | `wchar_t` |
| `u8'x'` | `UTF8` | `char8_t` (C++20) / `char` |
| `u'x'` | `UTF16` | `char16_t` |
| `U'x'` | `UTF32` | `char32_t` |

(Verified: `U'x'` dumps as `CharacterLiteral 'char32_t' 120`.)

**`StringLiteral`** is the same idea with content:

```cpp
SL->getString();        // StringRef — bytes, only valid for 1-byte element width!
SL->getBytes();         // raw storage regardless of width
SL->getLength();        // in characters
SL->getCharByteWidth(); // 1, 2, or 4
SL->getKind();          // Ordinary / Wide / UTF8 / UTF16 / UTF32 / Unevaluated
SL->getNumConcatenated();  SL->getStrTokenLoc(i);   // "a" "b" — one node, two tokens
```

Adjacent string literals concatenate into a *single* node during lexing, so
`"hello " "world"` is one `StringLiteral` remembering both token locations —
which matters the moment you rewrite one. `getString()` asserts on non-1-byte
widths; check `getCharByteWidth()` first (the same defensive pattern as
`getName()` vs `getNameAsString()` from Part 3).

## 15.3 — The keyword literals

Three trivial nodes, one trap:

- **`CXXBoolLiteralExpr`** — `true` / `false`, `getValue()`.
- **`CXXNullPtrLiteralExpr`** — `nullptr`; its existence is its value.
- **`GNUNullExpr`** — `__null`, the GNU builtin that `NULL` expands to in
  C++ on glibc/Darwin (verified: `void *p = NULL;` dumps a `GNUNullExpr
  'long'`).

The trap is the pair at the bottom: a modernizer migrating `NULL` →
`nullptr` is *precisely* the act of finding `GNUNullExpr` (or an integer
literal `0` in pointer context) and distinguishing it from
`CXXNullPtrLiteralExpr`. Two node kinds, one meaning, one century apart in
style — the AST keeps them apart so your tool can too.

## 15.4 — UserDefinedLiteral

`1.5_km` is a **call in literal's clothing** — and the AST says so.
Verified dump for `auto d = 1.5_km;`:

```
UserDefinedLiteral 'long double'
├─ImplicitCastExpr <FunctionToPointerDecay>
│ └─DeclRefExpr … 'operator""_km'
└─FloatingLiteral 1.500000e+00
```

`UserDefinedLiteral` *derives from* `CallExpr`: the callee is the
`operator""_km` function, the argument is the cooked literal. Extras on
top of the call interface:

```cpp
UDL->getLiteralOperatorKind();  // LOK_Floating, LOK_Integer, LOK_String,
                                // LOK_Character, LOK_Raw, LOK_Template
UDL->getCookedLiteral();        // the wrapped literal node (cooked forms)
```

This is why "find all calls" tools must remember that some calls don't look
like calls — `12_km`, `"abc"s`, and `2h + 30min` are all function calls, and
they all satisfy `callExpr()` matchers. (Part 18 returns to it from the call
side.)

## 15.5 — Compound and embedded literals

**`CompoundLiteralExpr`** — C99's `(int[]){1, 2, 3}` (a GNU extension in
C++): an unnamed object with a type and an initializer,
`getInitializer()` usually an `InitListExpr` (Part 19). In C it's an
lvalue you can take the address of; Clang models exactly that.

**`EmbedExpr`** — C23/C++26 `#embed "file.bin"`: the preprocessor directive
becomes a real expression node standing for the file's bytes
(`getDataStringLiteral()`, `getNumOfElements()`) rather than millions of
`IntegerLiteral`s — an AST-size optimization made visible. You'll meet it
only under `-std=c23`/`-std=c++26`, but when you do, nothing else looks
like it.

**Quiz.** `const char *s = "abc" "def";` — how many `StringLiteral` nodes,
and what do `getLength()`, `getCharByteWidth()`, and `getNumConcatenated()`
return? Then the sharper question: `L"abc" u"def"` — what happens?

> [!hint]- Hint
> Concatenation happens in the lexer, before the AST exists. And mixing
> *different* encoding prefixes is the interesting case.

> [!success]- Answer
> One node: `getLength() == 6`, `getCharByteWidth() == 1`,
> `getNumConcatenated() == 2` (each original token's location preserved via
> `getStrTokenLoc`). `L"abc" u"def"` is **ill-formed** — C++11 forbids
> concatenating different encoding prefixes — so no AST at all, just a
> diagnostic; with one prefix and one plain string (`L"abc" "def"`), you
> get a single wide `StringLiteral` of width `sizeof(wchar_t)`.
