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
calls. Constructor invocations and explicit or frontend-resolved cleanup
actions also contribute `Calls` with their source sites; existing `Construct*`
object/type relations remain unchanged. Cleanup edges are marked implicit for
automatic, temporary, and delete-triggered destruction. Lambda invocations
target the lambda's owned call-operator symbol.

## Querying

Select one root by qualified name or USR:

```text
facts-tool analyse call-graph -f facts.db -c project.db --function app::run
```

The default `--edges semantic` view classifies each stored edge as `function`,
`constructor`, `destructor`, `lambda`, or `virtual_dispatch`. Use
`--edges calls` for the compatibility view of the underlying `Calls` and
`DispatchCalls` primitives. Both views keep the stored relation kind.

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
`edge_view`, `roots`, `nodes`, and `edges`. Node evidence includes the stable
USR, resolved
declaration path, definition availability, a separate defining path/file/offset
object, outgoing-call presence, catalog `indexed`/`indexed_at` values,
freshness, the reserved failure member, coverage state, recommended action,
and candidate unindexed translation units. Coverage metadata follows the
definition file when one exists. Edge
evidence keeps `relation_kind`, `semantic_kind`, `implicit`, receiver name,
`receiver_type_id`, certainty, source location, cycle, reuse,
external-boundary, definition-boundary, and depth-truncation flags. Exact
receiver sites have a concrete receiver identity and certainty `exact`;
conservative sites use a null receiver identity and certainty `possible`.

Indirect calls or cleanup actions for which the frontend supplies no resolved
callable target are printed during extraction as
`coverage.unsupported_semantics` diagnostics with their source site. Schema v8
has no persistence field for these diagnostics, so JSON reports the coverage
member as `not-persisted` with an action to inspect extraction diagnostics; it
does not claim an empty persisted list is complete.

Emitted coverage states are `complete`, `incomplete`, `unknown`, `stale`, or
`not-applicable`. Missing catalog evidence is reported as `unknown`; it is not
silently upgraded to complete and does not trigger blind re-extraction when
definition or call facts already exist. The v1 `failure` member is reserved
and always null because the current stores persist no extraction-failure
record. The current import and extract commands also do not stamp `indexed`,
`indexed_at`, or `mtime`, so ordinary CLI-created pairs remain `unknown` until
another producer reconciles that catalog metadata. The complete/fresh/stale
fixtures explicitly simulate those catalog states. Without `--conf`, text
output retains opaque file identifiers and reports extraction coverage as
`unknown`.

Version 8 extends `relation_site` in place with nullable `receiver_type_id` and
`certainty` columns. The migration does not rebuild the table or add a context
table, and it preserves the existing primary and foreign keys.
