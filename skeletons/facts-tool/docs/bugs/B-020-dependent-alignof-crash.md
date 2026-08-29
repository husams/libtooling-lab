# B-020: dependent `alignof(T)` initializer crash

## Reproduction

An uninstantiated declaration such as

```cpp
template <typename T> struct DependentMember {
  static constexpr unsigned long value = alignof(T);
};
```

reached `clang::Expr::EvaluateAsRValue()` while the initializer still depended
on template substitution. With the pre-fix macOS LLVM 22 Release binary, the
focused BDD scenario terminated with signal 11 (`subprocess` return code `-11`,
equivalent to shell exit 139) during Clang AST extraction.

## Resolution

`evaluatedValue()` now rejects value-dependent, type-dependent,
instantiation-dependent, and unexpanded-parameter-pack expressions before
the constant evaluator is entered. `extractInitializer()` still records the
written expression, and the existing string-literal and general
`EvaluateAsRValue()` paths remain unchanged for non-dependent expressions.

Field, parameter-default, enumerator, and variable extraction all funnel
initializer expressions through `extractInitializer()` and its guarded
`evaluatedValue()` helper. The extractor sources contain only one direct
Clang constant-evaluator call, in `Initializer.cpp`, so the guard at that
shared choke point covers every site without parallel checks.

## Regression coverage

`initializer_dependent_alignment.feature` covers:

- successful extraction of uninstantiated class-member and variable-template
  `alignof(T)` initializers;
- persisted dependent expression text with `evaluated_kind = 'none'` and a
  NULL evaluated value;
- non-dependent `sizeof(AlignFixture)` and `alignof(AlignFixture)` values of 8;
- the genuine instantiated symbol
  `b020::DependentMember<b020::AlignFixture>::value`;
- stable symbol identities and an empty `PRAGMA foreign_key_check` result
  across repeated extraction.

The pre-fix focused run failed with signal 11. After the guard, all four B-020
scenarios passed.

## Platform gates

- macOS, Homebrew LLVM 22.1.8, Release: `facts-tool-cli-contract` and
  `facts-tool-e2e` passed; 165 BDD scenarios passed and one skipped.
- Rocky Linux 9, LLVM 21.1.8 with gcc-toolset-15: the repository build script
  passed all 23 CTest tests, then the standalone full E2E gate passed both
  registered executable tests.

The Backlog artifacts `e2e-report-b020-macos` and
`e2e-report-b020-rhel9` contain each platform's `bdd.xml`, `ctest.xml`, and
CTest log.
