# Workflow: Custom Matchers

## Goal

The stored relations that `extract` already gives you don't cover the
exact fact you need, for example one specific expression shape, or a
relation kind you want to persist yourself. You want to run a Clang dynamic
AST matcher directly against a translation unit, see it land correctly,
and understand why matcher breadth matters on a real, non-trivial file.

## Prerequisites

- `facts-tool match` requires a project database that already has the
  target source imported (see
  [02-importing-compile-commands](../02-projects-and-configuration/02-importing-compile-commands.md)).
- Familiarity with the binding contract in
  [03-extracting-facts/03-match-dynamic-matchers](../03-extracting-facts/03-match-dynamic-matchers.md):
  a matcher expression must bind exactly one of `symbol`, `call`+`callee`,
  or `source`+`target`[+`site`].

## Steps

### 1. A broad matcher can crash on a real, non-trivial translation unit

```console
$ facts-tool match --conf project.sqlite --facts facts.sqlite -v 2 \
    --matcher 'functionDecl(isDefinition()).bind("symbol")' src/commands/Import.cpp
symbol kind=method name=facts::config::Resolved::Resolved
...
facts-tool: cannot persist bound symbol 'facts::commands::mergedArguments(...)::(lambda)::(lambda at
  .../ConfigurationSupport.h:27:59)' usr='...': cannot extract symbol '...': invalid source location
$ echo $?
1
```

**What this tells you:** `functionDecl(isDefinition())` over an entire real
translation unit can walk into a nested-lambda declaration, a lambda's
implicit destructor defined inside another lambda, whose synthesized
location the extractor cannot persist. The whole `match` invocation fails
(exit 1) even though many symbols were bound and printed first.

### 2. Narrow the matcher: it succeeds and lands in the matched-symbol index

```console
$ facts-tool match --conf project.sqlite --facts facts.sqlite -v 1 \
    --matcher 'functionDecl(hasName("import"), isDefinition()).bind("symbol")' src/commands/Import.cpp
symbol kind=function name=facts::commands::(anonymous namespace)::import
facts-tool: 6 symbol(s) recorded from 5 file(s)
facts-tool: match: complete
$ facts-tool symbol find --conf project.sqlite --name 'import'
USR	QUALIFIED NAME	FILE ID	KIND	PATH	COMPONENT	REPOSITORY
c:Import.cpp@...  facts::commands::(anonymous namespace)::import  1  13  .../src/commands/Import.cpp  libtooling-lab  libtooling-lab
INDEX SCOPE: matched-only
SOURCE COMPLETE: unknown
```

**What this tells you:** adding `hasName(...)` to scope the matcher avoided
the nested-lambda sweep from step 1 entirely, and the bound symbol is
immediately visible through `symbol find`. Note the index header, though:
`INDEX SCOPE: matched-only` and `SOURCE COMPLETE: unknown`. This row is
discovery evidence that the declaration exists at this location, not proof
that its body or outgoing calls were ever extracted. See
[03-match-dynamic-matchers](../03-extracting-facts/03-match-dynamic-matchers.md#the-matched-symbol-index)
for the full contract of this index.

### 3. `call`+`callee` binding is also sensitive to matcher breadth

An unscoped call binding over the whole file fails:

```console
$ facts-tool match --conf project.sqlite --facts facts.sqlite -v 1 --relation-kind Calls \
    --matcher 'callExpr(callee(functionDecl().bind("callee"))).bind("call")' src/commands/Import.cpp
facts-tool: match: failed
facts-tool: direct call has no persistable call site
$ echo $?
1
```

Scoping to one function's body succeeds:

```console
$ facts-tool match --conf project.sqlite --facts facts.sqlite -v 1 --relation-kind Calls \
    --matcher 'callExpr(hasAncestor(functionDecl(hasName("main"))), callee(functionDecl().bind("callee"))).bind("call")' \
    src/main.cpp
argument index=0 source='argc' type='int' category=prvalue value='unknown'
argument index=1 source='argv' type='char **' category=prvalue value='unknown'
facts-tool: 2 symbol(s) recorded from 2 file(s)
facts-tool: match: complete
```

**What this tells you:** every `call`+`callee` match needs a persistable
call site behind it. Matching every `callExpr` in a real file can hit one
that has none; scoping the matcher with
`hasAncestor(functionDecl(hasName(...)))` keeps it to a body you already
know is well-formed for this purpose.

### 4. `source`/`target`/`site` binding: source and target must be declarations

```console
$ facts-tool match --conf project.sqlite --facts facts.sqlite -v 1 --relation-kind Uses \
    --matcher 'declRefExpr(to(parmVarDecl(hasName("argv")).bind("target")), \
                            hasAncestor(functionDecl(hasName("main")).bind("source"))).bind("site")' \
    src/main.cpp
relation kind=Uses source=main target=argv
facts-tool: 2 symbol(s) recorded from 1 file(s)
facts-tool: match: complete
```

An earlier attempt binding `source` directly to the `declRefExpr` itself
failed immediately:

```text
facts-tool: source and target bindings must be declarations
```

exit 1, no facts persisted.

**What this tells you:** `source` and `target` must each bind a
declaration node, `main`'s `functionDecl` and `argv`'s `parmVarDecl`
here, never the expression that references them. Bind the expression
itself to `site`. This matches the facts-tool-code-reasoning skill's
binding-contract discipline: "source/target/site are relation bindings and
require `--relation-kind`."

## What this tells you (summary)

Every fact `match` persists shows up through the same surfaces as ordinary
extraction: `symbol`-bound declarations appear in `symbol find`'s
matched-symbol index, and `call`/relation bindings persist relation rows
queryable the same way any other stored relation is (see
[05-impact-analysis-and-refactoring](05-impact-analysis-and-refactoring.md)
for querying stored relations). The difference is scope and intent:
`match` records exactly what your expression asks for, nothing more, which
makes it the right tool when a stock `extract` pass doesn't give you the
specific shape you need.

## Pitfalls

- **A broad matcher over a whole real file can crash the invocation.**
  `functionDecl(isDefinition())` and similar unscoped matchers can walk
  into a nested lambda's implicit destructor with an unpersistable
  synthesized location. Narrow with `hasName(...)` or
  `hasAncestor(functionDecl(hasName(...)))`.
- **An unscoped `callExpr` match can fail "direct call has no persistable
  call site."** Scope to one function's body with `hasAncestor(...)`.
- **`source`/`target` must bind declarations, never the expression.** Bind
  the expression to `site` instead; a near-miss binding shape fails
  contract validation before anything is persisted.
- **A `symbol find` hit from `match` is discovery evidence only.** It does
  not establish body, outgoing-call, or freshness coverage for that symbol.

## Where to go next

- [Match: Dynamic Matchers](../03-extracting-facts/03-match-dynamic-matchers.md)
  for the full binding contract, the matched-symbol index schema, and
  clearing index entries.
- [Inspecting Symbols from the CLI](../03-extracting-facts/05-inspecting-symbols-cli.md)
  for every `symbol find`/`symbol show` selector.
- [Impact Analysis and Refactoring](05-impact-analysis-and-refactoring.md)
  for querying the relations that both `extract` and `match` populate.
- [Using facts-tool from an AI Agent](08-using-facts-tool-from-an-ai-agent.md)
  for the same scope-narrowing discipline applied to an agent's own tool
  calls.
