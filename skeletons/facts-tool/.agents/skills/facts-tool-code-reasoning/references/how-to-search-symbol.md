# Global symbol lookup and flexible AST matching

Contents: [lookup](#find-existing-evidence),
[matcher scope and bindings](#match-the-smallest-useful-source-scope),
[relation roles](#map-semantic-roles-only-for-relation-persistence).

## Find existing evidence

Use the server's global index before reparsing source. Within an open
`facts_tool.rest.Client` context:

```python
from facts_tool.rest import SymbolKind

for symbol in client.symbols.find(
    qualified_name="example::Widget", kind=SymbolKind.CLASS,
):
    print(symbol.symbol_id, symbol.qualified_name, symbol.usr, symbol.definition)
```

Names default to case-sensitive literal prefixes: `example::Widget` can also
find `example::WidgetFactory`. Percent and underscore characters are literals.
Set `match="exact"` for name equality. Use `usr=...` for an exact USR.
Repository/component filters are optional; normal discovery is global.
This is the server's extracted-facts index, not the old CLI's match-only index.

Preserve all overload/repository candidates and select the intended identity.
Use `client.symbols.get(symbol_id)`, `.occurrences(symbol_id)`, and
`.relations(symbol_id, direction="outgoing")` for resource evidence. An
index miss does not establish absence; check readiness, registration, freshness,
and source scope. A symbol alone does not establish body/call coverage.

## Match the smallest useful source scope

Use typed source selections from [resource APIs](rest-api.md). Registered
headers can use an including TU's compiler context; missing or conflicting
contexts are analysis errors. Never guess header compiler flags.

```python
from facts_tool.rest import FileReference, FileSelection

selection = FileSelection(files=[
    FileReference(path="src/Widget.cpp", repository="example"),
])
job = client.matches.create(
    selection=selection,
    expression='cxxMethodDecl(hasName("run"), ofClass(hasName("Widget"))).bind("method")',
    traversal="IgnoreUnlessSpelledInSource",
    capture_source=True,
)
summary = job.wait(timeout=120)
for row in client.matches.results(job.id):
    method = row.bindings["method"]
    print(row.translation_unit, method.name, method.location)
```

Pass only the Clang expression, without `match` or `m` prefixes.
`AsIs` is the default traversal and includes implicit AST nodes;
`IgnoreUnlessSpelledInSource` skips nodes not spelled in source. Add
`isDefinition()` when definitions are required. Prefix lookup defaults do
not change Clang's `hasName` semantics. Each new match parses selected TUs;
narrowing a name does not turn matching into an index lookup.

Use arbitrary binding names, multiple bindings, helper bindings,
`equalsBoundNode`, `anyOf`, and `forEachDescendant`. Do not rename user
bindings to `symbol`, reject them, or restrict the Clang DSL to fixed shapes.
Examples of valid expressions:

```python
expression = 'functionDecl(hasName("main")).bind("entry")'
expression = 'returnStmt()'
expression = (
    'functionDecl(hasParameter(0, parmVarDecl().bind("parameter")))'
    '.bind("function")'
)
```

An expression with no explicit bindings returns the matched top-level node as
`root`. If it has explicit bindings, only those user bindings appear; an
explicit user binding named `root` is preserved. Inspect the actual binding
map rather than assuming a `symbol` key exists.

Without a relation kind, eligible named declarations can be persisted regardless
of binding name, including namespaces and aliases. Set `capture_source=True`
to capture supported expression/source evidence. Other nodes remain visible in
results even when they have no corresponding persistent fact. Preserve repeated
bindings in the result; storage eligibility does not restrict the binding map.
Storage errors still fail the operation; inspect the job and diagnostics.

## Map semantic roles only for relation persistence

`MatcherBindings` maps roles to the names already chosen in the expression.
It is not a whitelist of allowed bindings. For explicit relation persistence,
provide `relation_kind` and map any non-default role names:

```python
from facts_tool.rest import MatcherBindings

job = client.matches.create(
    selection=selection,
    expression=(
        'callExpr(hasAncestor(functionDecl(hasName("example::Service::run"))),'
        'callee(functionDecl().bind("destination"))).bind("invocation")'
    ),
    relation_kind="Calls",
    bindings=MatcherBindings(call="invocation", callee="destination"),
)
summary = job.wait(timeout=120)
```

The call role must resolve to a call expression and the callee to a function
declaration. For REST matching, explicitly set `relation_kind="Calls"` even
with the default `call`/`callee` names. The native CLI's implicit Calls shorthand
does not apply to the server's generic binding mode: without a relation kind,
the returned bindings alone do not request relationship persistence.

```python
job = client.matches.create(
    selection=selection,
    expression=(
        'cxxRecordDecl(hasName("example::Widget"), isDefinition(),'
        'isDerivedFrom(cxxRecordDecl().bind("baseClass"))).bind("derivedClass")'
    ),
    relation_kind="Inherits",
    bindings=MatcherBindings(source="derivedClass", target="baseClass"),
)
summary = job.wait(timeout=120)
```

Other relation kinds use declaration `source`/`target` roles and, where
required, an occurrence `site`. Map it with `MatcherBindings(site="use")`
when the expression binds that name. Extra helper bindings are allowed and
returned; relation mode persists endpoints and evidence rather than all helpers.
Names such as `source` alone do not reserve node types in ordinary matching.
Supply the intended relation kind when using role mappings to persist a relation.

Use [match result processing](match-results.md) for provenance and coverage;
use [call graphs](how-to-build-call-graph.md) when traversal is required.
