# Matching with Dynamic Matchers

`facts-tool match` runs a Clang **dynamic AST matcher expression** - the
same matcher language used by `clang-query` - against one or more
translation units, and persists whatever the matcher binds. Unlike
`extract`, which walks the whole AST and records everything it understands,
`match` records only what your matcher expression explicitly asks for.

```text
facts-tool match [OPTIONS] [sources...]

POSITIONALS:
  sources TEXT ...   Translation units relative to the invocation directory; defaults to imported order

OPTIONS:
  -f, --facts FILE          SQLite facts database; defaults to facts_template when omitted
      --matcher EXPR REQUIRED   Clang dynamic matcher expression; bind symbol, expression, call+callee, or source+target[+site]
      --format text|json       Located text (default) or structured invocation results
      --relation-kind KIND      Relation kind for source/target bindings; required for relation contracts
```

## Process located results

Normal symbol output includes `source=/absolute/path:line:column`. For a
structured collection containing only this invocation's matches, use JSON:

```sh
facts-tool match --format json \
  --matcher 'functionDecl(hasName("main")).bind("symbol")' src/main.cpp > matches.json
```

```python
from facts_tool import load_match_results

results = load_match_results("matches.json")
for match in results:
    symbol = match.bindings["symbol"]
    if symbol.location is not None:
        print(symbol.name, symbol.location.path,
              symbol.location.line, symbol.location.column)
```

`MatchResults.from_json(completed.stdout)` also accepts a native command's
captured stdout. Models expose each bound node's kind, name/USR when available,
location, byte range, and translation-unit path. A header matched from two TUs
keeps two occurrences. Unavailable locations are `None` with an explicit reason.
Coordinates refer to the physical expansion file, including when `#line`
changes the compiler's presumed file name or line number.
See the [Python match results reference](../../../python/docs/match-results.md).

JSON is emitted only after facts and the discovery index publish successfully;
a failed or cancelled invocation emits no successful result document. Compiler
and progress messages stay on stderr. An empty successful result has `matches=[]`.
`complete=true` means the matcher completed over the selected TUs; it does not
establish whole-project, function-body, or outgoing-call coverage. This JSON is
an invocation result, not another persisted database or a cached source index.

Matching validates included-file registration during the same frontend pass as
AST parsing. Multiple sources use separate Clang file managers and share one
facts transaction, so a later failure rolls back earlier matched facts. Each
new invocation still parses source; `symbol find` remains the fast lookup for
previously matched identities.

## The binding contract

For source-symbol discovery, start with a registered translation unit and
pass a Clang matcher expression directly to `--matcher` (without the
interactive `clang-query` prefix `match` or `m`):

```sh
facts-tool match \
  --matcher 'functionDecl(hasName("main")).bind("symbol")' src/main.cpp
facts-tool match \
  --matcher 'functionDecl(hasName("main"), isDefinition()).bind("symbol")' src/main.cpp
facts-tool symbol find --name main
```

These commands use the project and facts paths resolved from your YAML
configuration; inspect them with `facts-tool config show`. Add `--config FILE`
only to choose a specific YAML file. Replace the example source and name for
your project; explicit database paths are optional overrides.
The first expression finds matching function declarations; the second
requires a definition. `functionDecl` selects the AST node kind,
`hasName` narrows its name, and `isDefinition` narrows its form; all supplied
predicates must hold. `.bind("symbol")` identifies the declaration to record.
Search a header through a registered translation unit that includes it.
Use `cxxMethodDecl` with `ofClass`, as shown below, to narrow by class.

`match` parses source, while `symbol find` reads only previously matched
identities; resolve overloads by the returned USR. For additional persisted
evidence, always use the public Python SDK. Never query either database
directly, including for diagnostics. A matched definition still does not
establish extracted body or outgoing-call coverage.

A matcher expression must bind exactly one of four shapes:

- **`symbol`** - bind one declaration node to the name `"symbol"`. This is
  the shape used to find or confirm that a declaration exists.
- **`expression`** - bind an expression node to persist opt-in expression evidence.
- **`call`** + **`callee`** - bind a direct call expression and its callee,
  to record a `Calls` relation site.
- **`source`** + **`target`** (and optionally **`site`**) - bind an
  arbitrary relation between two nodes; `--relation-kind` is required for
  this shape, since the relation kind cannot be inferred from the matcher
  alone.

Binding `source` without a matching `target` fails contract validation -
every accepted binding shape needs its full node set, not a partial one.

## Worked example: binding a symbol by name

Bind every `Circle::area` declaration (interface and out-of-line
definition) by name, and persist both (paths and coordinates abbreviated below):

```console
$ facts-tool match -c demo.db -f ./demo-facts.db \
    --matcher 'cxxMethodDecl(hasName("area"), ofClass(hasName("Circle"))).bind("symbol")' \
    proj/src/shapes.cpp -v 1
facts-tool: match: starting
symbol kind=method name=shapes::Circle::area source=/.../shapes.cpp:<line>:<column>
symbol kind=method name=shapes::Circle::area source=/.../shapes.hpp:<line>:<column>
facts-tool: 2 symbol(s) recorded from 1 file(s)
facts-tool: match: complete
```

Note the `./` on the `--facts` path. `match` currently rejects a `--facts`
value that is a bare filename with no directory component, whatever the
YAML configuration says:

```console
$ facts-tool match -c demo.db -f demo-facts.db --matcher '...' proj/src/shapes.cpp
facts-tool: match: starting
facts-tool: match: failed
facts-tool: configuration error: cannot create facts_template directory: No such file or directory
```

Write `./demo-facts.db` or an absolute path and the same command succeeds.
`extract` is not affected; only `match` performs this check on an explicitly
supplied facts path. See
[limitations and known issues](../07-reference/04-limitations-and-known-issues.md).

A `symbol`-bound match persists symbol facts in the paired facts database and
also populates `matched_symbol_index` in the **project** database. The existing
SDK can read stored symbols and locations with `cb.get(usr)`; the new result
reader identifies exactly which nodes this invocation matched. Look up the
project discovery candidate with `symbol find`:

```console
$ facts-tool symbol find -c demo.db --name area
USR                                    QUALIFIED NAME          FILE ID  KIND  PATH                    COMPONENT  REPOSITORY
c:@N@shapes@S@Circle@F@area#1  shapes::Circle::area    2        17    .../proj/src/shapes.cpp  proj       proj
c:@N@shapes@S@Circle@F@area#1  shapes::Circle::area    820      17    .../proj/include/shapes.hpp  proj    proj
INDEX SCOPE: matched-only
SOURCE COMPLETE: unknown
```

Two rows for the same USR is expected: the index key is `(usr, file_id)`,
and the header declaration (file 820) and the out-of-line definition (file
2) each independently bound `"symbol"`, so each gets its own row.

## The matched-symbol index

`matched_symbol_index` lives in the project database and has exactly four
data columns: `usr`, `qualified_name`, `file_id`, and the raw Clang index
`kind`. Repository, component, and path columns shown by `symbol find` are
joined from the existing project catalog at query time.

Key properties:

- **Only `match` populates it.** Ordinary `extract` never populates,
  backfills, refreshes, or removes these rows.
- **Matching is additive.** A repeat match on the same `(usr, file_id)`
  updates the name and kind in place without duplicating the row; one USR
  can retain candidates from multiple registered files simultaneously.
- **An empty result is not proof of absence.** It only means no successful
  prior match recorded that candidate - the index is match-only, so a miss
  tells you nothing about whether the symbol exists in source. Likewise, an
  index row is not proof that a definition, body, calls, or a complete call
  graph is available for that symbol. JSON output makes this explicit with
  `coverage.index_scope="matched-only"` and `source_complete=null`.

`symbol find` selectors:

```console
facts-tool symbol find -c project.sqlite --usr 'c:@F@run#'
facts-tool symbol find -c project.sqlite --name 'app::run' --kind 13
facts-tool symbol find -c project.sqlite --name '%' --format json
```

Choose exactly one selector. `--usr` matches exactly; `--name` is a
case-sensitive, non-empty literal substring - `%` and `_` are ordinary
characters here, not SQL wildcards. Results are ordered by USR, then file
ID, across all compatible registered repositories and components by
default.

## Clearing the index

```console
$ facts-tool symbol index clear -c demo.db --file-id 2
Cleared 1 matched symbol candidate(s)

$ facts-tool symbol find -c demo.db --name area
USR                            QUALIFIED NAME        FILE ID  KIND  PATH                         COMPONENT  REPOSITORY
c:@N@shapes@S@Circle@F@area#1  shapes::Circle::area  820      17    .../proj/include/shapes.hpp  proj       proj
INDEX SCOPE: matched-only
SOURCE COMPLETE: unknown
```

This removes exactly the matched-symbol candidates registered under one
file ID: clearing file 2 (the source file) left the header's row (file 820)
untouched. Clearing an unknown or already-absent file ID is a successful
no-op that reports `Cleared 0 matched symbol candidate(s)` and exits 0.
`--file-id` must be a positive integer. Removing a file through the native
catalog command cascades its candidate rows automatically; ordinary
extraction never removes file identities from this index.

Use an explicit clear followed by a rematch when you need replacement
semantics - ordinary matching only ever adds or updates, it never erases
unrelated candidates on its own.

## Matcher scope matters

A matcher that is too broad can make `match` fail on a real translation
unit, even though the shape you bound is correct. Binding `symbol` to
every function definition in `main.cpp`, unscoped, walks into every
transitively included libc++ declaration, including the implicit
destructor of the file's own lambda:

```console
$ facts-tool match -c demo.db -f ./demo-facts.db -v 2 \
    --matcher 'functionDecl(isDefinition()).bind("symbol")' \
    proj/src/main.cpp
symbol kind=method name=std::integral_constant::operator type-parameter-0-0
...
symbol kind=method name=main()::<lambda@25:41>::operator()
symbol kind=method name=main()::<lambda@25:41>::~(lambda at .../proj/src/main.cpp:25:41)
facts-tool: cannot persist bound symbol 'main()::(lambda)::(lambda at .../proj/src/main.cpp:25:41)' usr='...': cannot extract symbol 'main()::(lambda)::(lambda at .../proj/src/main.cpp:25:41)': invalid source location
$ echo $?
1
```

Thousands of standard-library symbols print and bind successfully first;
the whole invocation still exits 1 and commits nothing, because the last
bound node (the lambda's synthesized closure destructor) has a source
location the extractor cannot persist. Narrow the matcher instead of
sweeping every declaration in the translation unit - add `hasName(...)`,
or scope with `hasAncestor(functionDecl(hasName(...)))`:

```console
$ facts-tool match -c demo.db -f ./demo-facts.db -v 1 \
    --matcher 'functionDecl(hasName("main"), isDefinition()).bind("symbol")' \
    proj/src/main.cpp
facts-tool: match: starting
symbol kind=function name=main
facts-tool: 1 symbol(s) recorded from 1 file(s)
facts-tool: match: complete
```

The same "narrow the matcher" rule applies to the `call`+`callee` and
`source`+`target` shapes below - each one fails differently when it is
too broad, and each one succeeds once scoped to one function's body.

## Binding a direct call: `call` + `callee`

`call`+`callee` records a `Calls` relation site between a call expression
and its resolved callee. `--relation-kind` is optional here (the shape
always means `Calls`), but if you do supply it, it must be exactly
`Calls` - any other value fails with `call and callee bindings only
support Calls`, exit 1. An unscoped matcher over the whole translation
unit fails immediately, before printing anything, because it reaches call
expressions (implicit conversions, cleanup calls) that have no call site
the extractor can persist:

```console
$ facts-tool match -c demo.db -f ./demo-facts.db --relation-kind Calls \
    --matcher 'callExpr(callee(functionDecl().bind("callee"))).bind("call")' \
    proj/src/main.cpp -v 1
facts-tool: match: starting
facts-tool: match: failed
facts-tool: direct call has no persistable call site
$ echo $?
1
```

Scoping the matcher to one function's body with `hasAncestor` succeeds:

```console
$ facts-tool match -c demo.db -f ./demo-facts.db --relation-kind Calls \
    --matcher 'callExpr(hasAncestor(functionDecl(hasName("totalArea"))), callee(functionDecl().bind("callee"))).bind("call")' \
    proj/src/main.cpp -v 1
facts-tool: match: starting
argument index=0 source=':' type='const class std::__wrap_iter<...>' category=lvalue value='unknown'
argument index=1 source=':' type='const class std::__wrap_iter<...>' category=lvalue value='unknown'
argument index=0 source='*item' type='const class shapes::Shape' category=lvalue value='unknown'
argument index=0 source='item' type='const class std::unique_ptr<class shapes::Shape>' category=lvalue value='unknown'
facts-tool: 16 symbol(s) recorded from 5 file(s)
facts-tool: match: complete
```

Each printed `argument index=...` line describes one bound call
expression's arguments at match time - this is diagnostic output, not what
gets persisted; the persisted evidence is the `Calls` relation site between
`totalArea` and each resolved callee reached inside its body (here, the
range-for's iterator operators and `shapes::describe`). Declaration nodes
bound by a matcher are added to the matched-symbol index the same way a
`symbol` bind is - `symbol find -c demo.db --name describe` returns a row
for `shapes::describe` after this run, even though this match never bound
anything to `"symbol"`.

## Binding an arbitrary relation: `source` + `target` (+ `site`) and `--relation-kind`

The `source`+`target` shape binds an arbitrary relation between two
**declarations**; `--relation-kind` is required, since the matcher alone
does not say which of the 23 relation kinds you mean. Bind the expression
itself to the optional `site` name, not to `source`/`target`. This example
records a `Uses` relation from `totalArea` to the range-for loop variable
`item`, scoped to one function:

```console
$ facts-tool match -c demo.db -f ./demo-facts.db --relation-kind Uses \
    --matcher 'declRefExpr(to(varDecl(hasName("item")).bind("target")), \
                            hasAncestor(functionDecl(hasName("totalArea")).bind("source"))).bind("site")' \
    proj/src/main.cpp -v 1
facts-tool: match: starting
relation kind=Uses source=(anonymous namespace)::totalArea target=item
facts-tool: 3 symbol(s) recorded from 2 file(s)
facts-tool: match: complete
```

Binding `source` directly to the expression instead of to a declaration
fails immediately, before any traversal output:

```console
$ facts-tool match -c demo.db -f ./demo-facts.db --relation-kind Uses \
    --matcher 'declRefExpr(to(varDecl(hasName("item")).bind("target"))).bind("source")' \
    proj/src/main.cpp -v 1
facts-tool: match: starting
facts-tool: match: failed
facts-tool: source and target bindings must be declarations
$ echo $?
1
```

`source` and `target` must each resolve to a declaration node
(`hasAncestor(functionDecl(...).bind("source"))` above binds the
*enclosing function declaration*, not the reference expression itself);
`site` is the only name allowed to bind the expression or occurrence node
directly.

## Summary of the binding contract

The `call`+`callee` and `source`+`target`[+`site`] binding forms, and the
`--relation-kind` option the latter requires, follow the same contract
described at the top of this chapter - bind exactly one complete shape per
matcher expression, and supply `--relation-kind` whenever you use the
`source`/`target` shape. If a matcher expression from this section doesn't
produce the expected persisted rows, confirm the binding names in your
expression exactly match `symbol`, `call`+`callee`, or `source`+`target`
(optionally `site`) - a near-miss binding name fails validation rather than
silently matching nothing. And if the matcher fails outright rather than
matching nothing, check whether it needs to be scoped down to one
function's body first, per the previous three sections.

`facts_committed` and `index_committed` describe successful publication (including
an empty no-op); they do not mean every binding became a discovery-index row.
The index accepts eligible named symbols. Successful expression bindings can
appear in results without becoming index rows; skipped-index diagnostics remain
on stderr. Unsupported implicit symbol persistence still fails the invocation
and produces no successful result document.
