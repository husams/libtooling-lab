---
name: facts-tool-code-reasoning
description: Use facts-tool and its Python query SDK to investigate, explain, or navigate C++ code from persisted symbols, relations, source locations, and project metadata.
---

# Facts-tool code reasoning

For every C++ reasoning task in this project, use facts-tool before drawing
conclusions. If valid facts cannot be produced, state the evidence gap and do
not present code-structure conclusions as confirmed.

1. Confirm that the facts and project SQLite databases belong to the code under
   investigation. Refresh them with the native CLI when they are missing or
   stale.
2. Query both databases through `facts_tool.open_codebase`; prefer a USR or an
   exact qualified name, then traverse stored relations.
3. Ground conclusions in returned names, kinds, locations, relations, sites,
   and result provenance. Never treat truncated, partial, unknown, or
   unsupported results as proof of absence.
4. Read source at returned locations to add local detail; keep the facts
   evidence visible in the explanation.

Read only the guide needed for the task:

- [Query C++ with Python](references/query-cpp.md)
- [Deploy in IPython-MCP](references/ipython-mcp.md)
- [Create and inspect databases with the native CLI](references/native-cli.md)

For the complete API, follow the links in
[`python/README.md`](../../../python/README.md), especially the language,
relations, views, model API, results, database, and troubleshooting pages.
