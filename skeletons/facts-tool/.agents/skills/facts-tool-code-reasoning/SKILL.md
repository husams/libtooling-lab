---
name: facts-tool-code-reasoning
description: Use facts-tool and its public Python SDK to search and reason about C++ code using persisted evidence or native AST match results with source locations.
---

# Facts-tool code reasoning

For every C++ reasoning task in this project, use facts-tool before drawing
conclusions. If valid facts cannot be produced, state the evidence gap and do
not present code-structure conclusions as confirmed.

**Never query either database directly.** All programmatic reads must use
the public Python SDK (`facts_tool`); never use SQL, `sqlite3`, database
drivers, private connections, or a diagnostic bypass. Native facts-tool
commands remain the interface for source matching, extraction, and graph
generation. If the SDK cannot expose evidence, report the capability gap.

1. Verify the executable and resolved configuration with `config show`; use
   native `symbol`, `match`, and `analyse call-graph` commands first so the
   project database and facts database remain a validated pair. Let YAML
   configuration resolve both paths by default; do not require the user to
   supply them or add `--conf`, `--facts`, or `--output` unnecessarily.
   Use `--config FILE` only to select a specific YAML file, and explicit
   database paths only for intentional overrides or isolated fixtures.
2. Reuse existing facts, identify missing evidence, and refresh only the
   smallest required translation units with native `import`/`extract` or
   `match`; do not use SQL, database drivers, or duplicate SDK callgraph
   traversal.
3. For an existing name or USR, try `symbol find` before parsing source;
   an index miss is not proof of source absence. Use `match` for requested AST
   predicates or missing/refreshed evidence, scoped to candidate registered
   TUs. Every invocation parses those TUs; narrowing a name does not avoid
   parsing. Use `match --matcher '...bind("symbol")'` for symbols and exact
   `call`/`callee` bindings for direct Calls; `source`/`target`/`site` are
   relation bindings and require `--relation-kind`.
4. Ground conclusions in native names, kinds, locations, relations, sites, and
   explicit boundary or failure diagnostics; symbol-only matching does not
   establish outgoing call coverage.
5. Use the installed Python SDK for persisted graph runs, expressions, field
   effects, ancestors, and bounded source regions; do not scan source files or
   replace a missing fact with an inferred answer.
6. For the exact result collection from a match invocation, use `--format json`
   and public `MatchResults`/`load_match_results`, following
   [match result processing](references/match-results.md). Verify these
   capabilities in the selected executable and Python environment; checkout
   documentation does not establish that an installed release provides them.
   Preserve binding names, TU provenance, optional coordinates, and publication
   flags; a later discovery-index lookup is not the invocation result set.

## Agent workflow

Use the paired-store workflow in [agent workflows](references/agent-workflows.md).
It covers native symbol/match/call-graph commands, SDK evidence queries,
freshness and coverage flags, bounded paging, and clean installed-package
acceptance. Keep one concise sentence per query and record tool-call/output
metrics when an acceptance harness provides them.

The S-028 skill refinement remains separately owned; this skill links its
native-first disposition and does not claim S-028 acceptance.

## How to

Use the [user guide](../../../docs/user-guide/toc.md) for command options
and worked examples; read the relevant workflow before running commands.

1. **Build a call graph:** follow
   [Generating a call graph](../../../docs/user-guide/04-call-graphs/02-generating-a-call-graph.md)
   to run `analyse call-graph` with the YAML-resolved project and facts databases
   and an exact function name or USR. Read the persisted SQLite run through
   the [public SDK reader](../../../docs/user-guide/05-python-sdk/06-persisted-callgraph-runs.md)
   and distinguish traversal completion from source coverage; see also the
   [agent call-graph workflow](references/how-to-build-call-graph.md).
2. **Search source code with the Clang matcher DSL:** follow the
   [step-by-step source-symbol search](references/how-to-search-symbol.md)
   and
   [Matching with dynamic matchers](../../../docs/user-guide/03-extracting-facts/03-match-dynamic-matchers.md)
   and bind the requested declaration as `symbol`, for example
   `functionDecl(hasName("main")).bind("symbol")`. Target registered source
   candidates and retain the returned identity and location; a symbol match
   does not establish body or outgoing-call coverage. Use the
   [symbol-search workflow](references/how-to-search-symbol.md) to query the
   matched-symbol index and resolve ambiguous names by USR.
3. **Extract facts for missing information:** follow
   [Extracting facts](../../../docs/user-guide/03-extracting-facts/01-extract.md)
   to refresh the smallest required set of translation units with
   `extract SOURCE...` using the configured database paths; first
   [import their real compile commands](../../../docs/user-guide/02-projects-and-configuration/02-importing-compile-commands.md)
   if they are not registered. Include related sources together when their
   caller entries must remain available, as described under re-extraction.
   For missing graph evidence during traversal, follow
   [Recovery and boundaries](../../../docs/user-guide/04-call-graphs/04-recovery-and-boundaries.md)
   for `--recover-missing`. Re-query the evidence after extraction or recovery
   and report any remaining stale, missing, or unsupported information.

Read only the guide needed for the task:

- [Query C++ with Python](references/query-cpp.md)
- [Process native match results with Python](references/match-results.md)
- [Deploy in IPython-MCP](references/ipython-mcp.md)
- [Create and inspect databases with the native CLI](references/native-cli.md)

For the complete API, follow the links in
[`python/README.md`](../../../python/README.md), especially the language,
relations, views, model API, results, database, and troubleshooting pages.
