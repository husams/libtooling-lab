---
name: facts-tool-code-reasoning
description: Use the facts-tool Python REST wrapper to search the server's global C++ symbol index, extract facts, run flexible Clang AST matchers, build call graphs, track variables across functions, and manage registered repositories and analysis jobs.
---

# Facts-tool code reasoning

For C++ reasoning tasks, obtain facts-tool evidence before drawing conclusions.
If valid evidence cannot be produced, report the gap and qualify the answer.

Use `facts_tool.rest.Client` or `AsyncClient` and their typed **v2 resource
namespaces** for extraction, matching, discovery, analysis, and administration.
The server owns project/facts databases, caches, and the global index. Clients
supply a server URL and optional token, then resource identities or selections;
never ask for database paths for a REST operation.

Do not shell out to analysis commands, parse CLI output, construct CLI argument
arrays, or use the legacy flat v1 methods for these workflows. Reserve the
native executable for starting/deploying the server when needed. Never query
SQLite directly, use database drivers/private SDK connections, or infer missing
facts by scanning source. The local read-only SDK is an explicit fallback only
for evidence unavailable through REST and accessible in an existing local
artifact; see [querying evidence](references/query-cpp.md).

## Workflow

1. Verify the installed `facts-tool-query[rest]` package and server connection.
   Inspect `client.server.health()`, `client.server.readiness()`, and
   `client.index.status()`. Listener health alone does not establish index or
   repository readiness. Follow [agent workflows](references/agent-workflows.md).
2. Reuse registered repositories. Registration and server startup schedule
   discovery, import, extraction, and index publication when monitoring is
   enabled. Await `client.repositories.wait_until_ready(repo.id, timeout=...)`
   before relying on automatic processing. Compilation databases come from the
   real build system; do not invent compiler settings. Use typed import/scan
   jobs when manual processing is needed.
3. Start name lookup with `client.symbols.find(qualified_name=...)`. Search is
   global across registered repositories, with optional kind/repository/component
   filters. Names use case-sensitive literal **prefix** matching by default;
   use `match="exact"` only for equality. USRs are exact. Resolve ambiguous
   candidates with returned IDs or USRs, never by taking the first row.
4. Refresh only missing/stale evidence with `client.extractions.create(...)`;
   use `client.matches.create(...)` for AST predicates and exact occurrences.
   Select the smallest sufficient registered source set. Use explicit
   `AllSelection()` only when all sources are intended. Index misses are not
   proof of source absence.
5. Pass Clang matcher expressions unchanged. Arbitrary names, multiple/helper
   bindings, and unbound roots are supported. No `.bind("symbol")` requirement
   exists. Use `MatcherBindings` only to map semantic relation roles when
   needed; see [symbol search and bindings](references/how-to-search-symbol.md).
6. Wait for the actual job, retain its ID, and read its typed result collections
   lazily. `.wait()` returns a summary; `None` collections mean not fetched,
   not empty. Inspect diagnostics, coverage, and boundaries before answering.
   Process the exact match job's rows, not a later index lookup; see
   [match results](references/match-results.md).
7. Use `client.callgraphs` for callers, callees, and paths, and
   `client.variable_flow` for local/parameter reads, writes, and cross-function
   value flow. Reuse a suitable retained job rather than recomputing traversal.
   A successful job or matched declaration does not establish full source or
   outgoing-call coverage.
8. Report concise findings with identities, locations, job IDs, scope, and
   relevant limits. Do not dump full result collections or compiler traces.

## Task references

Read only what the task needs:

- [Connection, freshness, lazy results, async clients, and failures](references/agent-workflows.md)
- [Repository/file management, import, scan, extraction, dependencies, and server APIs](references/rest-api.md)
- [Global lookup and flexible Clang matchers](references/how-to-search-symbol.md)
- [Typed matcher results and provenance](references/match-results.md)
- [Call graphs and paths](references/how-to-build-call-graph.md)
- [Local variables and parameters](references/variable-flow.md)
- [Evidence queries and limited local SDK fallback](references/query-cpp.md)
- [IPython-MCP sessions](references/ipython-mcp.md)

Verify installed capabilities against the [Python REST guide](../../../python/docs/rest-v2.md),
[resource contract](../../../docs/user-guide/09-rest-api/02-requests-and-jobs.md),
and [OpenAPI guide](../../../docs/user-guide/09-rest-api/08-openapi-contract.md).
Use current wrapper signatures and typed results; do not invent a REST method
from a similarly named local SDK or CLI feature.
