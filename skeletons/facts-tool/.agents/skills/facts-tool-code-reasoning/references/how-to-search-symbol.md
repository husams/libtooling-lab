# How to search source code with the Clang matcher DSL

Use `facts-tool match` to parse registered C++ translation units and find
declarations with Clang's dynamic AST matcher language, the same DSL used
by `clang-query`. Pass only the expression to `--matcher`, without the
interactive `clang-query` prefix `match` or `m`.

## 1. Select the project and source files

Confirm the executable and effective configuration with
`facts-tool config show`. Let the discovered YAML configuration resolve the
project database and its paired facts database; neither path needs to be
supplied on each command. Use `--config ./team.yaml` on commands only when
selecting a specific YAML file. Source paths are relative to the invocation
directory; replace the example source paths and names below for the task.

If the source is not registered, first
[import its real compile commands](../../../../docs/user-guide/02-projects-and-configuration/02-importing-compile-commands.md).
Search headers through a registered translation unit that includes them;
do not invent a header's compiler flags or ownership.

## 2. Write and run a declaration matcher

For a simple name or USR lookup, query the existing matched-symbol index first:

```sh
facts-tool symbol find --name main
facts-tool symbol find --usr 'EXACT_USR'
```

If the index has the required candidate, use that identity with the public SDK
without reparsing. A miss is only a gap in match history, so an explicit AST
predicate, or a lookup that needs fresh occurrence evidence, still uses `match`
over the smallest registered translation-unit set that can answer it.

Find declarations named `main` in a selected source:

```sh
facts-tool match \
  --matcher 'functionDecl(hasName("main")).bind("symbol")' src/main.cpp
```

`functionDecl(...)` selects function declarations (including methods),
`hasName(...)` narrows the name, and `.bind("symbol")` tells facts-tool which
declaration to record. Bind exactly `symbol`, not an arbitrary name. Quote
the expression with shell single quotes and matcher strings with double
quotes. If an explicit facts-path override is needed, use `--facts ./facts.db`
or an absolute path, as described in the
[match guide](../../../../docs/user-guide/03-extracting-facts/03-match-dynamic-matchers.md).
The default text result includes the bound node's source path, line, and
column; use the [structured match-results reference](match-results.md) when
the exact invocation result must be retained.

To find only definitions, add `isDefinition()`:

```sh
facts-tool match \
  --matcher 'functionDecl(hasName("main"), isDefinition()).bind("symbol")' src/main.cpp
```

To find a method on a specific class, combine `cxxMethodDecl`, `hasName`,
and `ofClass`:

```sh
facts-tool match \
  --matcher 'cxxMethodDecl(hasName("area"), ofClass(hasName("Circle"))).bind("symbol")' src/shapes.cpp
```

Add `isDefinition()` inside `cxxMethodDecl(...)` when only method bodies
are wanted. Multiple predicates in one matcher must all match. Supply more
source paths when needed; omitting source paths searches every imported
translation unit, so prefer a bounded candidate set first.

## 3. Resolve or reuse a discovered symbol identity

When identity lookup or reuse is useful, query the discovery index through the
native CLI:

```sh
facts-tool symbol find --name main
facts-tool symbol find --usr 'EXACT_USR'
```

Retain USR, qualified name, kind, and source path for every candidate. A
name can match multiple overloads; select the intended exact USR instead
of choosing the first row. A declaration and definition may produce
different file rows for the same USR.

`symbol find` searches the matched-symbol index; it does not parse source.
Only successful `match` populates that index. Ordinary `extract` does not.
A miss is not proof the symbol is absent everywhere; check the source scope,
matcher, and diagnostics.

An exact JSON result already records the invocation's bound identities and
coordinates; it does not require a follow-up index query unless you need an
index candidate for later reuse or overload disambiguation.

## 4. Obtain additional evidence through supported interfaces

A `symbol` binding establishes identity and location, not complete body,
outgoing-call, or freshness coverage. Extract the required registered
translation units when that evidence is missing, then use the
[Python SDK query guide](query-cpp.md) for persisted evidence and bounded
source regions. Never query either database directly, including through SQL,
`sqlite3`, another database driver, or private SDK connections.

For call occurrences, use the separate `call` + `callee` binding contract;
for arbitrary relations, bind declaration `source` + `target`, optional
expression `site`, and supply `--relation-kind`. Follow the scoped examples
in [Custom matchers](../../../../docs/user-guide/06-workflows/06-custom-matchers.md)
instead of treating a symbol search as relation extraction. Broad matchers
can encounter unpersistable implicit nodes; inspect the exit status and
diagnostics, and treat a failed invocation as an evidence gap. See
[match-results.md](match-results.md) for the successful JSON contract and
Python reader.

Use the resolved name or USR in the
[call-graph workflow](how-to-build-call-graph.md) when a graph is needed.
