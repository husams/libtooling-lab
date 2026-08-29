# B-021 — Owner relations and unnamed references abort extraction

Verified on macOS 15 (arm64), Homebrew LLVM 22.1.8, `facts-tool` at `bcdc26f`
(`build/`, Release). Reproduced from `cmapp/ngidc/src/service/BroadcastOptimizations.cpp`
in the `cml/base` workspace and reduced to `tests/fixtures/e2e/relation_resolution.cpp`.

Two independent defects, both surfacing as `indexing incomplete`.

## 1. Owner symbols are assumed to be persisted

`storeMethodRelation`, `storeFieldOwnerRelation`, and `enumerationId` looked the
owner up with `FactStore::findId` and treated a miss as fatal:

```
cannot persist relation=method_of source='std::hash<regression::Hashable>::operator()'
  target='std::hash' usr='c:@N@std@S@hash>#$@N@regression@S@Hashable':
  target symbol is not persisted
```

The owner is genuinely absent whenever the member is defined in the main file
but its record is declared somewhere extraction filters out — a system header,
or a generated header missing from the registry. `BroadcastOptimizations.cpp`
reaches that shape through its generated include graph.

`RecordDecl.cpp:81-97` already resolved base-class targets on demand through
`findOrStoreSymbolTarget`, which persists an `ExternalBit` stub keyed by USR.
`Storage::saveSymbol` (`src/storage/Symbol.cpp:200-218`) is USR-keyed, so a
later real save of the same owner reuses the stub's `SymbolId` and
`is_external=MIN(is_external,excluded.is_external)` clears the flag. Method,
field, and enumerator owners now take the same path.

## 2. An unnamed reference was escalated to a fatal invalid USR

`BodyVisitor::capture` turned every `ExtractionError::InvalidUsr` into an
`indexing incomplete` record. Clang refuses a USR for unnamed declarations, and
a `MemberExpr` reaching a member of an anonymous union or anonymous struct
resolves through the unnamed intermediate field:

```
cannot extract reference to
  'regression::Reading::regression::Reading::(anonymous union at ...)': invalid USR
```

Such a reference is skipped, not failed.

### The predicate has to be the declaration name, not the printed one

The first fix keyed the skip on `referenced.getQualifiedNameAsString().empty()`.
Measured against LLVM 22, that string is empty only for an unnamed parameter at
translation-unit scope. Every other un-USR-able declaration prints a placeholder:

| declaration | `getDeclName()` empty | qualified name | USR fails |
| --- | --- | --- | --- |
| unnamed parameter | yes | `` | yes |
| unnamed bit-field | yes | `Named::(anonymous)` | yes |
| anonymous union field | yes | `S::S::(anonymous union at …)` | yes |
| anonymous struct field | yes | `S::S::(anonymous struct at …)` | yes |
| anonymous namespace | yes | `(anonymous)` | no |
| unnamed struct / enum | yes | `(unnamed struct at …)` | no |

With the `.empty()` predicate the anonymous-union access still aborted the unit.
The check is `!referenced.getDeclName()`, which covers the whole class; the
`InvalidUsr` conjunct keeps the named-but-USR-able rows out.

## 3. Fixture — `tests/fixtures/e2e/relation_resolution.cpp`

`relation_resolution.hpp` carries `#pragma GCC system_header`, so every owner it
declares is filtered out of the store. The translation unit defines the members
out of line and reads through an anonymous union and an anonymous struct,
covering both defects in one unit.

Observed on `bcdc26f` before the fix — five diagnostics, exit code 1:

```
indexing incomplete: cannot persist relation=field_of  source='regression::Box::value'        … target symbol is not persisted
indexing incomplete: cannot persist relation=field_of  source='regression::BoxedPair::value'  … target symbol is not persisted
indexing incomplete: cannot persist relation=method_of source='std::hash<regression::Hashable>::operator()' … target symbol is not persisted
indexing incomplete: cannot extract reference to '…(anonymous union at …)': invalid USR
indexing incomplete: cannot extract reference to '…(anonymous struct at …)': invalid USR
```

After the fix — exit code 0, `16 symbol(s) recorded from 2 file(s)`, with the
three owner relations pointing at `is_external=1` stubs.

## 4. Coverage

`tests/e2e/features/b019_extraction_completeness.feature` gains *Template owner
relations survive an external specialization*, asserting the clean exit, one
`method_of` edge into `std::hash`, and two `field_of` edges into the `Box`
records. The scenario fails on `bcdc26f` (`expected extract exit code 0, got 1`)
and passes with the fix.
