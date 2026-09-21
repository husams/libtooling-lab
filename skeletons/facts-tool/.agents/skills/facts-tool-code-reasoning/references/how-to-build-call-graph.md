# Build or reuse a call graph through Python

Resolve the intended function using [global lookup](how-to-search-symbol.md).
Pass a `SymbolReference` containing a returned symbol ID, exact USR, or exact
qualified name. Analysis roots/targets are exact identities; prefix search
defaults do not make an analysis root a prefix. Disambiguate overloads or
cross-repository names rather than taking the first candidate.

## Submit the graph

Inside an open client context:

```python
from facts_tool.rest import SymbolReference

job = client.callgraphs.create(
    root=SymbolReference(qualified_name="example::Service::run"),
    direction="callees",
)
summary = job.wait(timeout=120)
print(job.id, summary.coverage, summary.truncated, summary.truncation_reason)
for node in client.callgraphs.nodes(job.id):
    print(node.symbol_id, node.qualified_name, node.external,
          node.unresolved_calls, node.pointer_calls)
for edge in client.callgraphs.edges(job.id):
    print(edge.source, edge.target, edge.file_id, edge.line, edge.column)
for boundary in client.callgraphs.frontier(job.id):
    print(boundary.symbol_id, boundary.reason)
for diagnostic in client.callgraphs.diagnostics(job.id):
    print(diagnostic.severity, diagnostic.message)
```

Use `direction="callers"` for incoming traversal. The default traversal has
no requested cap; supply `max_depth`, `max_nodes`, `max_edges`, or
`time_limit_ms` when a bounded investigation is intended, and disclose those
limits. `wait(timeout=...)` limits client waiting, not server traversal.

Job summaries contain counts and coverage; fetch nodes/edges/frontier separately
and lazily. Resolve an edge's symbol IDs against nodes from the same job and
its file ID through `client.files.get(...)` when a path is needed. Preserve
edge direction, depth, cycles, implicit sites, external/definition boundaries,
and unresolved/pointer-call counts.

## Ask for a path

```python
path_job = client.callgraphs.create(
    root=SymbolReference(qualified_name="example::Service::run"),
    target=SymbolReference(qualified_name="example::save"),
    direction="callees",
    path_mode="shortest",
)
path_summary = path_job.wait(timeout=120)
for path in client.callgraphs.paths(path_job.id):
    print(path.nodes)
```

Use `path_mode="all_simple"` for all simple paths when required. Inspect the
actual paths collection, coverage, truncation, frontier, and diagnostics.
A nonempty edge collection alone does not prove the requested target was
reached; an empty path collection with limits or unresolved boundaries does
not prove source-level unreachability. Do not read local-reader-only
`path_outcome`/`target_reached` attributes from the REST summary.

## Refresh evidence and reuse jobs

Use `client.callgraphs.get(job_id)` to reuse a retained job matching the
requested scope and source state. Keep the server job ID; do not parse native
stdout, assume a run ID of 1, open a graph database, or duplicate the traversal
through ad hoc relation queries.

If the root or required body evidence is missing/stale, inspect the catalog,
import real compilation commands when needed, run targeted
`client.extractions.create(...)`, and then submit the graph. The v2 callgraph
wrapper does not accept the CLI's `recover_missing`, `calls_scope`, or
database options; do not invent them.

A successful traversal is not proof of complete extraction or external behavior.
Report partial coverage, recovery/definition gaps, and retained boundary
evidence with the result.
