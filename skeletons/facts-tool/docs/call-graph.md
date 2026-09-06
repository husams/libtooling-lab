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
facts-tool analyse call-graph -f facts.db -c project.db --function app::run
```

Or list every definition-backed root with calls:

```text
facts-tool analyse call-graph -f facts.db -c project.db --all
```

`--max-depth N` adds a positive traversal cap. Without it, traversal continues
until a cycle, a reused context, or an external symbol. External boundaries are
complete stops and are not reported as truncation. Output ordering is canonical
and each edge reports relation kind, receiver context, source location, cycle
reuse, external-boundary, and depth-truncation state.

`-c/--conf` supplies the matching project catalog. When present, the command
validates the project/facts file identities, resolves source paths, and reports
pair-aware extraction evidence. Stored traversal completion remains the
top-level `complete` value; it never implies extraction coverage. A
project-local declaration without a stored definition is
`definition-availability=project-missing`, while a genuinely unavailable
third-party target remains an `external-boundary`.

Use `--format json` for the stable `facts-tool.call-graph.v1` representation.
It contains `complete`, `truncated`, `traversal`, `extraction_coverage`,
`roots`, `nodes`, and `edges`. Node evidence includes the stable USR, resolved
source path, definition availability, outgoing-call presence, catalog
`indexed`/`indexed_at` values, freshness, recorded failure evidence, coverage
state, recommended action, and candidate unindexed translation units. Edge
evidence keeps relation kind, receiver certainty, source location, cycle,
reuse, external-boundary, definition-boundary, and depth-truncation flags.

Coverage states are `complete`, `incomplete`, `unknown`, `stale`, `failed`, or
`not-applicable`. Missing catalog evidence is reported as `unknown`; it is not
silently upgraded to complete and does not trigger blind re-extraction when
definition or call facts already exist. `failure` is null when the paired
stores contain no recorded failure evidence. Without `--conf`, text output
retains opaque file identifiers and extraction coverage is `unknown`.

Version 8 extends `relation_site` in place with nullable `receiver_type_id` and
`certainty` columns. The migration does not rebuild the table or add a context
table, and it preserves the existing primary and foreign keys.
