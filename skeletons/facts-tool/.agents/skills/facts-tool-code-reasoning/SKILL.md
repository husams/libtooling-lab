---
name: facts-tool-code-reasoning
description: Use facts-tool and its Python query SDK to investigate, explain, or navigate C++ code from persisted symbols, relations, source locations, and project metadata.
---

# Facts-tool code reasoning

For every C++ reasoning task in this project, use facts-tool before drawing
conclusions. If valid facts cannot be produced, state the evidence gap and do
not present code-structure conclusions as confirmed.

1. Verify the executable and resolved configuration with `config show`; use
   native `symbol`, `match`, and `analyse call-graph` commands first so the
   project database and facts database remain an explicit pair.
2. Reuse existing facts, identify missing evidence, and refresh only the
   smallest required translation units with native `import`/`extract` or
   `match`; do not use SQL, database drivers, or duplicate SDK callgraph
   traversal.
3. Use `match --matcher '...bind("symbol")'` for symbols and exact
   `call`/`callee` bindings for direct Calls; `source`/`target`/`site` are
   relation bindings and require `--relation-kind`.
4. Ground conclusions in native names, kinds, locations, relations, sites, and
   explicit boundary or failure diagnostics; symbol-only matching does not
   establish outgoing call coverage.
5. Use the installed Python SDK for persisted graph runs, expressions, field
   effects, ancestors, and bounded source regions; do not scan source files or
   replace a missing fact with an inferred answer.

## Agent workflow

Use the paired-store workflow in [agent workflows](references/agent-workflows.md).
It covers native symbol/match/call-graph commands, SDK evidence queries,
freshness and coverage flags, bounded paging, and clean installed-package
acceptance. Keep one concise sentence per query and record tool-call/output
metrics when an acceptance harness provides them.

The S-028 skill refinement remains separately owned; this skill links its
native-first disposition and does not claim S-028 acceptance.

## How to

- [Build a call graph](references/how-to-build-call-graph.md)
- [Search for a symbol](references/how-to-search-symbol.md)

Read only the guide needed for the task:

- [Query C++ with Python](references/query-cpp.md)
- [Deploy in IPython-MCP](references/ipython-mcp.md)
- [Create and inspect databases with the native CLI](references/native-cli.md)

For the complete API, follow the links in
[`python/README.md`](../../../python/README.md), especially the language,
relations, views, model API, results, database, and troubleshooting pages.
