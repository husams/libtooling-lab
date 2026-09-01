# Contextual call graphs

Extraction records `Calls` for statically selected callees, `Overrides` from a
derived method to its base declaration, and conservative `DispatchCalls` for
virtual targets. Each site keeps its source location. A proven concrete
by-value receiver stores its type with `exact` certainty; pointer, reference,
implicit, and otherwise unproven receivers store no type and use `possible`.
No `unknown` certainty is persisted.

The extractor builds Clang's translation-unit call graph once after ordinary
body extraction. `CallGraphVisitor` only orchestrates focused call-site,
receiver, and override extractors. Symbols are joined by canonical USR, so a
declaration-only callee can become the same definition-backed symbol when a
later translation unit supplies its body. Constructor bodies contribute their
calls, while constructor invocations remain represented only by the existing
`Construct*` relations.

## Querying

Select one root by qualified name or USR:

```text
facts-tool analyse call-graph -f project.db --function app::run
```

Or list every definition-backed root with calls:

```text
facts-tool analyse call-graph -f project.db --all
```

`--max-depth N` adds a positive traversal cap. Without it, traversal continues
until a cycle, a reused context, or an external symbol. External boundaries are
complete stops and are not reported as truncation. Output ordering is canonical
and each edge reports relation kind, receiver context, source location, cycle
reuse, external-boundary, and depth-truncation state.

Version 8 extends `relation_site` in place with nullable `receiver_type_id` and
`certainty` columns. The migration does not rebuild the table or add a context
table, and it preserves the existing primary and foreign keys.
