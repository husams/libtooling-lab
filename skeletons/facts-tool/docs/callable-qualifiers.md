# Callable qualifiers (B-031)

Functions, function templates and instances share semantic property extraction.
Instance methods also retain const, volatile and ref qualifiers. Constructors,
destructors, conversions and lambda operators use the applicable shared stages.
Method ownership relations retain the established behavior.

`is_noexcept` means proven non-throwing, not merely a written noexcept keyword.
False and dependent specifications remain false; instantiated functions carry
their own evaluated result. Lazy trivial special members are non-throwing;
other unresolved exception specifications remain false until Clang resolves
those specifications. Extraction never forces template instantiation.
`is_explicit` likewise records a true semantic explicit condition; dependent
conditions are not assumed true. Constant evaluation is an enum, not bit flags.

Schema version 9 adds `symbol.is_volatile`, backed by previously unused bit 25.
All earlier bit assignments remain unchanged. Opening storage for extraction
migrates supported old schemas and preserves existing rows and identities.
Migrated rows initially contain zero for volatility: historical extraction did
not record the property, so that zero is unknown historical metadata rather
than evidence the source was non-volatile. Re-extract all relevant translation
units before relying on qualifiers in an old database, including const/ref and
other callable fields that older tools left unset. Catalog readers are read-only;
perform migration through extraction before using an old database with them.

List, detail and the shared browser declaration formatter use const, volatile,
ref and noexcept suffix order. Detail flags expose other supported specifiers;
constant evaluation has its own field. Parameters retain their stored spelling
and defaults, and variadic declarations display the ellipsis.

## Regression map

- AC1/5: `callable_qualifiers.feature`: all 12 cv/ref combinations, exact
  per-overload USRs and static/free parameter/return qualifier controls.
- AC2/3: `callable_specifiers.feature`: exception specifications, constexpr,
  consteval, variadics, linkage, inline, virtual/pure/override/final and deletion.
- AC2/4/5: conditional templates/instances, explicit constructors/conversions,
  defaulted/deleted special members, implicit exception specifications and
  ordinary/generic const/mutable lambdas. True/false controls match exact USRs.
- AC5/6/7: `callable_persistence.feature`: reopen, repeat extraction, reverse
  TU order, migration from version 8 and fresh semantic parity. Parameter
  source spans may reflect either redeclaration; parameter contracts are stable.
- AC8: list/detail qualifier/default rendering, template/special-member
  formatting and read-only catalog behavior; `symbol-browser` tests exercise
  the same declaration formatter used by the interactive browser.
- AC9/10: run the complete pytest-bdd harness through CTest on LLVM 22 macOS
  and LLVM 21 Linux, alongside the full native CTest suite.
- AC7: storage schema tests retain fresh round-trip and legacy migration checks.
- AC11/12: count every changed file; new text files must be at most 100 physical
  lines, and an independent reviewer verifies the tested implementation head.

Validation commands (from this skeleton):

```
cmake --build build
ctest --test-dir build --output-on-failure
ctest --test-dir build -R "^facts-tool-e2e$" --output-on-failure
SKIP_DEPS=1 scripts/build-rhel9.sh
```
