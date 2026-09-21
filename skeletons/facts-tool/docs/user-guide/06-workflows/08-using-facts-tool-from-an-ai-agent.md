# Workflow: Using facts-tool from an AI Agent

← [User guide index](../README.md) · [Table of contents](../toc.md)

Use the project's
[facts-tool-code-reasoning skill](../../../.agents/skills/facts-tool-code-reasoning/SKILL.md)
to answer C++ questions through the typed Python REST wrapper. The running
server owns the project catalog, facts stores, and global symbol index. Agents
need its listener URL and optional token, not database paths or CLI commands.

## Connect and reuse indexed evidence

Install `facts-tool-query[rest]` in the agent's Python environment. Use the
actual server address in place of the example:

```python
from facts_tool.rest import Client

with Client("http://127.0.0.1:42817") as client:
    print(client.server.readiness())
    for symbol in client.symbols.find(qualified_name="example::Service"):
        print(symbol.symbol_id, symbol.qualified_name, symbol.definition)
```

Keep the client context open for subsequent examples. Lookup defaults to
case-sensitive literal prefixes; use `match="exact"` for equality or
`usr=...` for an exact identity. Optional kind, repository, and component
filters narrow the global search. Resolve overloads and repository collisions
before analysis. This index includes extracted facts; it is not the native
CLI's older match-only index.

Registration and startup perform automatic import/extraction when monitoring
is enabled. After registering or changing the active clone, use
`client.repositories.wait_until_ready(repo.id, timeout=120)`.
Readiness, watcher failures, and missing compiler settings matter: an empty
lookup is not proof of source absence. See [resource APIs](../09-rest-api/02-requests-and-jobs.md).

## Refresh or match a selected source

```python
from facts_tool.rest import FileReference, FileSelection

selection = FileSelection(files=[
    FileReference(path="src/Service.cpp", repository="example"),
])
extraction = client.extractions.create(selection=selection, force=False)
extracted = extraction.wait(timeout=120)

match_job = client.matches.create(
    selection=selection,
    expression='functionDecl(hasName("example::Service::run")).bind("entry")',
    traversal="IgnoreUnlessSpelledInSource",
)
matched = match_job.wait(timeout=120)
for row in client.matches.results(match_job.id):
    print(row.translation_unit, row.bindings["entry"].location)
```

Refresh only missing/stale evidence. Use real build compilation commands.
Arbitrary names, multiple/helper bindings, and unbound roots are supported;
`.bind("symbol")` is not required. Without explicit bindings, the returned
top-level node is named `root`. Map custom relation roles using
`MatcherBindings` with `relation_kind`; see the skill's
[matcher recipe](../../../.agents/skills/facts-tool-code-reasoning/references/how-to-search-symbol.md).

## Ask for a call path

```python
from facts_tool.rest import SymbolReference

job = client.callgraphs.create(
    root=SymbolReference(qualified_name="example::Service::run"),
    target=SymbolReference(qualified_name="example::save"),
    direction="callees",
    path_mode="shortest",
)
summary = job.wait(timeout=120)
print(job.id, summary.coverage, summary.truncated, summary.truncation_reason)
for path in client.callgraphs.paths(job.id):
    print(path.nodes)
for boundary in client.callgraphs.frontier(job.id):
    print(boundary.symbol_id, boundary.reason)
for diagnostic in client.callgraphs.diagnostics(job.id):
    print(diagnostic.severity, diagnostic.message)
```

Use returned symbol IDs or USRs when names are ambiguous. Reuse a suitable
retained job with `client.callgraphs.get(job_id)`. Apply depth/node/edge/time
limits when a bounded investigation is intended, and report them. A client
wait timeout does not cancel server work.

Read the paths collection to establish a requested path; nonempty edges alone
do not prove target reachability. Preserve partial coverage, external or
definition boundaries, unresolved calls, and truncation. Use the separate
`client.variable_flow` analysis for local/parameter reads, writes, argument
passing, and returns; an ordinary call graph does not establish variable flow.

## Keep results lazy and conclusions precise

Job polling returns scalar summaries. Large record fields can be `None`
until fetched through `results`, `nodes`, `edges`, `paths`,
`boundaries`, or `diagnostics`; this does not mean empty evidence.
Collections fetch pages as needed. `limit` sets page size, not an overall cap.
Use `itertools.islice` for a bounded preview, or `.collect()` when eager
materialization is intended. Keep the client open while iterating.

Never query SQLite directly, parse analysis CLI output, infer missing evidence
by scanning source, or mix a previous job's result with the current request.
Report identities, relevant source sites, job ID, scope, and limitations
concisely. A successful job or symbol match does not prove complete source
coverage.

See the [Python REST client](../05-python-sdk/11-rest-client.md) and the skill's
[variable-flow recipe](../../../.agents/skills/facts-tool-code-reasoning/references/variable-flow.md)
for further examples and asynchronous clients.
