# Query C++ evidence through Python

Use the typed REST wrapper for all operations it exposes. The distribution is
`facts-tool-query[rest]`; import `Client`/`AsyncClient` from
`facts_tool.rest`. The server resolves stores and the global index.

## Start from an identity

Inside an open client context:

```python
candidates = client.symbols.find(
    qualified_name="example::Service::run", match="exact",
).collect()
if len(candidates) != 1:
    raise ValueError("Resolve the intended overload/repository before continuing")
symbol = candidates[0]
for occurrence in client.symbols.occurrences(symbol.symbol_id):
    print(occurrence.kind, occurrence.path, occurrence.line, occurrence.column)
for relation in client.symbols.relations(
    symbol.symbol_id, kind="Calls", direction="outgoing",
):
    print(relation.source.qualified_name, relation.target.qualified_name,
          relation.count)
```

The small exact-identity candidate collection is deliberately eager; use lazy
iteration for broad lookup/results. Prefix matching remains the default for
normal discovery. Relation records provide endpoint identities/counts, not
individual call-site coordinates; use graph edges or matcher rows for sites.
Do not infer a direct occurrence from a relationship count alone.

## Select the evidence API

| Question | API and recipe |
| --- | --- |
| Names, kinds, definitions, overload candidates | `client.symbols.find/get`; [lookup](how-to-search-symbol.md) |
| Declaration/definition locations | `client.symbols.occurrences` |
| Stored incoming/outgoing relationships | `client.symbols.relations` |
| Requested AST shape or exact occurrence | `client.matches`; [match results](match-results.md) |
| Callers, callees, reachability paths | `client.callgraphs`; [call graphs](how-to-build-call-graph.md) |
| Local/parameter reads, writes, value flow | `client.variable_flow`; [variable flow](variable-flow.md) |
| File ownership and compiler settings | `client.files/components/directories/repositories`; [resources](rest-api.md) |
| Missing facts or include dependencies | `client.extractions/dependencies`; [resources](rest-api.md) |

Ground answers in returned identities, sites, directions, job scope, and
coverage. Use typed diagnostics/errors to identify gaps. Re-query after
targeted extraction rather than assuming refreshed evidence has a particular
value. Do not scan source or infer absent edges as confirmed behavior.

## Evidence not yet exposed by REST

The local read-only SDK remains available for features such as declarative
query plans, expression details, field effects, ancestors, and bounded source
regions that do not have equivalent v2 resource methods. Do not invent remote
`client.field_writers`, `client.definition_regions`, or a generic SQL API.

If the task requires such evidence and an existing local paired artifact is
already accessible, use the public local reader explicitly:

```python
from facts_tool import open_codebase

# Existing, resolved local artifact paths; never send these to the server.
with open_codebase(facts_db=facts_path, project_db=project_path) as cb:
    evidence = cb.field_writers("example::Box::value")
    print(evidence.truncated, evidence.partial, evidence.unknown)
```

Keep this fallback separate from normal server workflows. Do not demand the
server's database paths, copy its live stores, or replace supported REST
operations with CLI/local access. If no suitable artifact exists, report the
REST capability gap. Never use SQL, `sqlite3`, database drivers, or private
connections, including for diagnostics.

Consult the local [model API](../../../../python/docs/model-api.md),
[results](../../../../python/docs/results.md), and
[troubleshooting](../../../../python/docs/troubleshooting.md) only for this
fallback. Check freshness/provenance and reopen the local reader after external
writes when necessary. An old local snapshot is not current server evidence.
