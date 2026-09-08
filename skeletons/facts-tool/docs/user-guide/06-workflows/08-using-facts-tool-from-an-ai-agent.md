# Workflow: Using facts-tool from an AI Agent

## Goal

An AI agent needs to answer a question like "how does X reach Y" against a
real C++ codebase, using as few tool calls and as little printed output as
possible, while never presenting an unverified guess as a confirmed fact.
This chapter is the low-token sequence the project's own
`facts-tool-code-reasoning` skill prescribes for exactly that situation,
validated end to end on a small, deterministic fixture.

## Prerequisites

- The repository's `facts-tool-code-reasoning` agent skill (at
  `.agents/skills/facts-tool-code-reasoning/` in this checkout) and its
  `references/` guides. This chapter restates the skill's discipline; read
  the skill itself for the authoritative wording.
- A paired project/facts database that has already been extracted for the
  function(s) in question (see
  [01-onboarding-a-codebase](01-onboarding-a-codebase.md)).

## Steps

### 1. Confirm the resolved database pair first, quietly

```console
$ facts-tool config show --conf mini/project.db | head -5
```

**What this tells you:** one quick call catches a stale-cache or
wrong-project surprise (see
[07-large-codebases-and-batching](07-large-codebases-and-batching.md#4-confirm-the-resolved-configuration-before-running-anything-at-scale))
before you spend a call querying against the wrong facts.

### 2. Prefer `symbol show` over `symbol find` for a known name

```console
$ facts-tool symbol find --conf mini/project.db --name 's025_leaf'
USR	QUALIFIED NAME	FILE ID	KIND	PATH	COMPONENT	REPOSITORY
INDEX SCOPE: matched-only
SOURCE COMPLETE: unknown
```

An empty result here does **not** mean the symbol is absent. `symbol
find` only searches the matched-symbol index, and only `match` populates
that index, never plain `extract` (see
[06-custom-matchers](06-custom-matchers.md)). For an exact-name lookup
against extracted facts, the common case, use `symbol show` instead:

```console
$ facts-tool symbol show s025_leaf --facts mini/facts.db --conf mini/project.db
s025_leaf() -> int
  source     s025_workflow.cpp:1:5
```

**What this tells you:** an agent that treats a `symbol find` miss as
"symbol does not exist" will draw a wrong conclusion. Reserve `symbol
find` for match-only discovery questions and use `symbol show` for
ordinary extracted-fact lookups.

### 3. Run the call graph budgeted, at `-v 0`, for one completion line

```console
$ facts-tool analyse call-graph --conf mini/project.db --facts mini/facts.db \
    --function main --to s025_leaf -v 0
facts-tool: call graph run 4 complete
```

**What this tells you:** this one line is the entire native output,
regardless of how large the underlying graph is. On any codebase whose
size you don't already know, add `--max-depth`/`--max-nodes`/`--max-edges`/
`--time-limit-ms` so a runaway traversal can't consume unbounded time or
memory in the agent's own process either. See
[../04-call-graphs/02-generating-a-call-graph](../04-call-graphs/02-generating-a-call-graph.md#budgets---max-depth---max-nodes---max-edges---time-limit-ms).

### 4. Read the answer from the persisted run, paged and narrow

```python
run = cb.callgraphs.get(4, limit=5)          # bounded page, not the whole run
print(run.path_outcome, run.path_found)      # check this BEFORE reading edges as a path
for e in run.edges:                          # only 5 edges materialized
    print(e.source.qualified_name, e.target.qualified_name)
# run.edges.next_cursor lets you page further only if needed
```

**What this tells you:** the actual answer to "how does X reach Y" lives
in the run just persisted, not in the CLI's one-line stdout from step 3.
Read it back with `cb.callgraphs.get()`, capped with `limit`, and page
with `next_cursor` only if more edges are actually needed. Always check
`path_outcome`/`path_found` before treating a non-empty edge list as proof
of a path: a forward `--to` search can report `unreachable` while a
reverse `--direction callers` walk from the same target looks reachable,
when an unresolved call boundary separates the two directions. See
[03-tracing-calls-and-paths](03-tracing-calls-and-paths.md#5-reverse-callers-and-a-path-query-that-comes-back-unreachable)
for that exact case on a real codebase, and
[../04-call-graphs/04-recovery-and-boundaries](../04-call-graphs/04-recovery-and-boundaries.md)
for why it happens.

### 5. Keep the whole session terse

The facts-tool-code-reasoning skill states this directly: "Return one
short sentence unless details are requested; do not print full manuals,
query objects, source, or compiler traces." An agent answering "how does X
reach Y" should report the path (or its absence) and the run ID it came
from, not the full transcript of commands that produced it.

## Minimal-token checklist

Distilled from the steps above:

1. Run `config show` once to confirm the resolved `--conf`/`--facts` pair.
2. Prefer `symbol show <exact name>` over `symbol find` for extracted
   facts; reserve `symbol find` for match-only discovery questions.
3. Run `analyse call-graph` with `-v 0` and explicit budgets
   (`--max-depth`/`--max-nodes`/`--time-limit-ms`) so a runaway graph
   can't blow up your own process either.
4. Read the run with `cb.callgraphs.get(run_id, limit=N)` and `select()`ed
   queries, never unbounded iteration; page with `next_cursor` only as
   needed.
5. Always check `path_outcome`/`truncated`/`partial`/`unknown` before
   asserting an answer. A "complete" run with edges is not proof of a
   found path, and a truncated count is `None`, not zero.

## What an agent must never do

Per the skill's evidence and output limits, an agent reasoning about C++
structure with facts-tool must never:

- **Query SQLite directly, use a database driver, or otherwise bypass the
  native CLI or public Python SDK** to inspect call-graph or symbol
  tables. Use a documented public reader (`facts-tool` commands, or
  `CodeBase`/`cb.callgraphs`); if it can't answer the question, report the
  gap rather than reaching for the database file directly.
- **Read or scan source files to answer a C++ structure question.** If
  valid facts cannot be produced from stored evidence, state the evidence
  gap and do not present code-structure conclusions as confirmed.
- **Recompute or duplicate a persisted call-graph traversal** through ad
  hoc relation navigation (`cb.graph.callees()`/`callers()`) when a run
  already answers the question. Read the persisted run with
  `cb.callgraphs.get()` instead of re-deriving the same edges.
- **Treat a matched-symbol-index hit, or a non-empty edge list on an
  unreachable-outcome run, as proof of coverage or a found path.** A
  symbol match alone never establishes outgoing-call coverage.
- **Substitute checkout source for the installed CLI/SDK via
  `PYTHONPATH`, or rebuild/reinstall without authorization.** Report the
  exact gap instead.

## Pitfalls

- **A `symbol find` miss is not proof of absence.** The matched-symbol
  index is match-only.
- **An unbudgeted call-graph run on a large codebase can be expensive for
  the agent's own process.** Budget every `analyse call-graph`
  invocation whose graph size you don't already know.
- **A "complete" path run with a non-empty edge list is not proof of a
  found path.** Always check `path_outcome` before asserting
  reachability.
- **Default SDK enumeration budgets silently truncate an aggregate to
  `None`, not zero.** Check `.truncated` before trusting a count on a
  large codebase.

## Where to go next

- [Persisted Call-Graph Runs](../05-python-sdk/06-persisted-callgraph-runs.md)
  for the full `CallGraphReader`/`CallGraphRun` API used in step 4.
- [Recovery and Boundaries](../04-call-graphs/04-recovery-and-boundaries.md)
  for the frontier, boundary, and recovery semantics an agent should check
  before asserting a graph is complete.
- [Troubleshooting](../07-reference/03-troubleshooting.md) for what to do
  when a facts-tool call fails outright.
- [Impact Analysis and Refactoring](05-impact-analysis-and-refactoring.md)
  for the same evidence-grounding discipline applied to impact analysis.
