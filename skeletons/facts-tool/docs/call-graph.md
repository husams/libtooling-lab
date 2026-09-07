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
Use [missing-evidence recovery](call-graph-recovery.md) for the explicit
`--recover-missing` request across registered components.

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

Traversal has no application cap by default: it continues across every
registered component and available library edge until a cycle, reused context,
or external symbol provides a semantic stop. Optional request-only controls are:

- repeatable `--component NAME`, selecting the named component union;
- `--calls-scope all|project|library`, defaulting to `all`;
- positive `--max-depth`, `--max-nodes`, `--max-edges`, and
  `--time-limit-ms` budgets.

Component and project/library filters require the matching project catalog.
They cut at excluded endpoints, report observed boundary identities, and never
walk through an excluded node to reconnect a permitted one. Selected roots are
subject to the same endpoint filters, so an out-of-scope root is reported as
excluded and `--calls-scope library` needs a library-side root. Node budgets
count canonical symbol identities, edge budgets count canonical relation keys,
depth counts call-edge hops, and time uses a monotonic clock. The root counts
as one node. An exact-depth leaf with no qualifying outgoing edge is complete.

Every reached budget reports its exact reason and discovered-but-unexpanded
frontier while setting traversal coverage incomplete. SIGINT emits a coherent
partial result when possible, reports `cancelled`, and exits 130. Filters,
counters, timers, and cancellation state are released at process exit; neither
the facts database nor project catalog is used as a result cache.

Structural-budget frontier entries are discovered endpoints that were not
admitted or expanded. For time limits and cancellation, the frontier also
includes the admitted node whose expansion stopped and every remaining eligible
selected root; filtered roots remain in `excluded_scope` instead.
These controls also apply to reverse callers and path queries.

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
It contains `complete`, `query`, `coverage`, `truncation`, `excluded_scope`,
`recovery`, `errors`, `traversal`, `extraction_coverage`, `roots`, `nodes`, and
`edges`, `edge_view`, `paths`, and `path_result`.
`query` echoes explicit scope and nullable limits; `truncation` names
the reason and frontier; `excluded_scope` reports filters plus observed node
and edge identities/counts.
`query.mode` is `callees`, `callers`, or `path`; path results distinguish `found`,
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

## Portable output and recovery lifecycle

```sh
facts-tool analyse call-graph --conf project.db --facts facts.db \
  --function main --recover-missing --format mermaid --output main-callgraph.mmd
```

`--format text|json|mermaid` defaults to text. An omitted `--output` writes
stdout; an explicit file is replaced atomically with a sibling temporary file.
Input database and registered source aliases are rejected as output targets.
If `--facts` is omitted, the validated configuration must provide a
project-scoped `facts_template`; configuration selection is resolved once and
passed to recovery as a concrete project path.

For Mermaid file output with recovery, an initial diagram with a visible
partial/pending label is published before recovery begins. Changed graph
generations replace that diagram; the final publication reports recovery
results. There is one native traversal per graph generation, shared by
recovery selection and rendering. Text and JSON emit one final document.
Progress uses stderr. A recovery failure leaves a valid diagram labelled
partial and returns exit 1. Invalid configuration or selectors do not replace
an existing artifact. A successful stored traversal does not imply complete
extraction or fresh source evidence.

If recovery is interrupted, the final artifact retains the last usable graph
generation and marks it cancelled and incomplete (exit 130). Newly extracted
facts can remain in the database even when cancellation prevents their graph
from being traversed; the artifact does not claim to include that later evidence.
Operational errors in JSON stdout mode produce one JSON error document and
exit 1 without a duplicate stderr diagnostic.

Mermaid uses pair-scoped stable node IDs and escaped labels. Native edges,
shared nodes, cycles, callable semantics, and call sites survive rendering.
Its JSON header comment preserves root USRs, source/facts provenance, coverage,
selected query controls, frontiers, excluded identities and recovery errors.
The [runnable skill recipe](../.agents/skills/facts-tool-code-reasoning/references/how-to-build-call-graph.md)
uses a checked-in fixture; [symbol search](../.agents/skills/facts-tool-code-reasoning/references/how-to-search-symbol.md)
explains exact selection and match-only index evidence.
