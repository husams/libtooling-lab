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

Use [function entries](call-graph-entries.md) to distinguish committed body
generation, missing entries, generated leaves, and retained external targets.

Select one root by qualified name or USR:

```text
facts-tool analyse call-graph -f facts.db -c project.db --function app::run
```

The default `--edges semantic` view classifies each stored edge as `function`,
`constructor`, `destructor`, `lambda`, or `virtual_dispatch`. Use
`--edges calls` for the compatibility view of the underlying `Calls` and
`DispatchCalls` primitives without semantic classification. Both views keep
the stored relation kind and traverse the same stored edge set; only their
presentation differs.

Or list every definition-backed root with calls:

```text
facts-tool analyse call-graph -f facts.db -c project.db --all
```

Reverse callers and between-symbol paths are opt-in:

```text
facts-tool analyse call-graph -f facts.db -c project.db \
  --function app::run --direction callers
facts-tool analyse call-graph -f facts.db -c project.db \
  --function app::start --to app::finish --path-mode all-simple
```

`callees` remains the default direction. Path mode defaults to `shortest`,
which returns one minimum-hop path using canonical USR and relation-site
tie-breaks. `all-simple` returns every deterministically ordered node-simple
path, so cycles cannot produce infinite results; a source equal to its target
is a valid zero-edge path. Exact qualified names and USRs are accepted.
Ambiguous names report every candidate identity and exit 2 so a USR can be
selected. `--direction callers` cannot be combined with `--to`, `--to` cannot
be combined with `--all`, and `--path-mode` requires `--to`.

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
`edge_view`, `query`, `paths`, `path_result`, `roots`, `nodes`, and `edges`.
`query.mode`
is `callees`, `callers`, or `path`; path results distinguish `found`,
`not_found`, `unknown`, and `truncated`, and each path carries stable string
node IDs and canonical relation-site edge keys. A `truncated` result can retain
paths found before the requested cap; it does not imply an empty path array.
`not_found` is emitted only
for complete relevant extraction evidence; missing metadata or unresolved
boundaries remain `unknown`. All query state is request-local and creates no
cache or schema. Node evidence includes the stable USR, resolved
declaration path, definition availability, a separate defining path/file/offset
object, outgoing-call presence, catalog `indexed`/`indexed_at` values,
freshness, the reserved failure member, coverage state, recommended action,
and candidate unindexed translation units. Coverage metadata follows the
definition file when one exists. Edge
evidence keeps `relation_kind`, `implicit`, receiver name, `receiver_type_id`,
certainty, source location, cycle, reuse,
external-boundary, definition-boundary, and depth-truncation flags. Exact
receiver sites have a concrete receiver identity and certainty `exact`;
conservative sites use a null receiver identity and certainty `possible`.
Semantic-view edges additionally include `semantic_kind`; calls-view edges
omit that derived classification.

Indirect calls or cleanup actions for which the frontend supplies no resolved
callable target are printed during extraction as
`coverage.unsupported_semantics` diagnostics with their registered project-file
source site. These coverage diagnostics use the logging facility's always-on
level and therefore remain visible at `--verbose 0`; unregistered system files
are suppressed. Schema v8 has no persistence field for these diagnostics, so
JSON reports the coverage member as `not-persisted` with an action to inspect
extraction diagnostics; it does not claim an empty persisted list is complete.

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
