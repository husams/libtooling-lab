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

Traversal has no application cap by default: it continues across every
registered component and available library edge until a cycle, reused context,
or external symbol provides a semantic stop. Optional request-only controls are:

- repeatable `--component NAME`, selecting the named component union;
- `--calls-scope all|project|library`, defaulting to `all`;
- positive `--max-depth`, `--max-nodes`, `--max-edges`, and
  `--time-limit-ms` budgets.

Component and project/library filters require the matching project catalog.
They cut at excluded endpoints, report observed boundary identities, and never
walk through an excluded node to reconnect a permitted one. Node budgets count
canonical symbol identities, edge budgets count canonical relation keys, depth
counts call-edge hops, and time uses a monotonic clock. The root counts as one
node. An exact-depth leaf with no qualifying outgoing edge is complete.

Every reached budget reports its exact reason and discovered-but-unexpanded
frontier while setting traversal coverage incomplete. SIGINT emits a coherent
partial result when possible, reports `cancelled`, and exits 130. Filters,
counters, timers, and cancellation state are released at process exit; neither
the facts database nor project catalog is used as a result cache.

`-c/--conf` supplies the matching project catalog. When present, the command
validates the project/facts file identities, resolves source paths, and reports
pair-aware extraction evidence. Stored traversal completion remains the
top-level `complete` value; it never implies extraction coverage. A
project-local declaration without a stored definition is
`definition-availability=project-missing`, while a genuinely unavailable
third-party target remains an `external-boundary`.

Use `--format json` for the stable `facts-tool.call-graph.v1` representation.
It contains `complete`, `query`, `coverage`, `truncation`, `excluded_scope`,
`recovery`, `errors`, `traversal`, `extraction_coverage`, `roots`, `nodes`, and
`edges`. `query` echoes explicit scope and nullable limits; `truncation` names
the reason and frontier; `excluded_scope` reports filters plus observed node
and edge identities/counts. Node evidence includes the stable USR, resolved
declaration path, definition availability, a separate defining path/file/offset
object, outgoing-call presence, catalog `indexed`/`indexed_at` values,
freshness, the reserved failure member, coverage state, recommended action,
and candidate unindexed translation units. Coverage metadata follows the
definition file when one exists. Edge
evidence keeps relation kind, receiver certainty, source location, cycle,
reuse, external-boundary, definition-boundary, and depth-truncation flags.

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
