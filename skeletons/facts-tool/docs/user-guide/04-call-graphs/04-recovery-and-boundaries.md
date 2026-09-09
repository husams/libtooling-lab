# Recovery and Boundaries

This chapter covers the two ways `analyse call-graph` deals with gaps in
stored evidence: an explicit, opt-in **recovery** request that can fill in
missing call evidence, and the different kinds of **boundary** the
traversal reports when it genuinely cannot go further.

## `--recover-missing`: what it does

Without `--recover-missing`, `analyse call-graph` is strictly read-only.
With it, the command may extract additional evidence during the traversal:

```console
$ facts-tool analyse call-graph -c project.db -f facts.db --function app::run --recover-missing
```

Recovery works from the selected project/facts pair and registered
translation units, including other registered components - it never
invents compiler flags or header ownership, so register the real compiler
commands for any TU you want recovery to consider before requesting it.

### How a candidate is found and validated

1. The [matched-symbol index](../03-extracting-facts/03-match-dynamic-matchers.md#the-matched-symbol-index)
   supplies candidates by canonical USR. An absent index hit does **not**
   prove a definition is unavailable - registered compiler commands remain
   discovery candidates independent of the match index.
2. A symbol-only match establishes identity and a location; it does not
   supply body or call evidence. Full extraction is what supplies that.
3. When freshness is unknown, recovery validates the current definition
   extent and call occurrences by running the native extractor on the
   retained AST into an **isolated, temporary facts store**, then compares
   its canonical graph evidence against what is already stored. This
   temporary store is removed after collection - it never replaces your
   facts database, and at default verbosity it emits no extractor progress
   or coverage notices, and never claims to have recorded symbols into your
   facts.
4. Existing non-stale body entries support reuse of derived virtual-dispatch
   evidence; unresolved call boundaries remain explicit rather than being
   guessed at.

A missing entry alone does not force extraction - only incomplete or
changed evidence triggers recovery work. `--max-depth` limits both recovery
and reused-evidence reporting the same way it limits ordinary traversal.

### The recovery record: `callgraph_run_recovery`

Every attempted translation unit is recorded, one row per TU, in
`callgraph_run_recovery(run_id, tu_file_id, outcome, diagnostic)`.
`outcome` is one of `attempted`, `failed`, `reused`, or `suppressed`;
`diagnostic` holds the reason, including the captured compiler output when
a TU fails to compile. `reused` covers symbols grouped under a registered
owning TU, including header bodies. **This table is the durable record -
there is no separate JSON `recovery` object anywhere in the CLI.**

### Recovery failure

A requested recovery failure completes the run with `status=recovery-failed`,
exits 1, and prints exactly one stderr line:

```text
facts-tool: recovery failed for N translation unit(s); see callgraph_run_recovery run <id>
```

The run keeps every edge reached **before** the failing TU - for example,
`root -> bridge` is retained even if `bridge -> leaf` could not be recorded
because the leaf's translation unit failed to compile. A successful
recovery search can still leave an unavailable definition boundary; read
the public SDK's `cb.callgraphs.get(run_id).recovery` and `.edges` pages rather than treating exit
0 as proof of complete source coverage.

### Retry scope

Retry/dedup tracking exists only within **one invocation** - a fresh
process has no memory of prior failures. Identity for suppressing a
redundant retry includes the canonical store pair, the TU, its driver, its
working directory, its effective ordered argv, the registry fingerprint,
and a SHA-256 of its input content. Unchanged failed attempts and no-match
searches covering the current wanted-USR subset are suppressed within that
one invocation; changed input, configuration, or newly wanted USRs can
permit a new attempt, but facts writes alone do not. Unreadable inputs are
reported as gaps, not silently skipped.

### Verbosity

Recovery progress (`facts-tool: recovery-start` / `recovery-complete`) only
prints at `-v 1` or higher, alongside extraction summaries and compiler
diagnostics. At `-v 3`, a
`facts-tool: recovery validation tu=<id> reason=...` line also appears -
useful for synchronizing a SIGINT during recovery with what was in
progress. At the default verbosity, none of this progress prints.

## External boundaries

A **boundary** is a call target whose declaration is known but whose
definition is not (yet) in the project. This is recorded via
`callgraph_external_reference(source_id, destination_id, kind, position,
file_id, offset, external_symbol_id)`, preserving the caller's real call
site and the unresolved target's symbol identity so it can be resolved
later if the definition is ever extracted from another component.

A boundary is a **complete stop, not a truncation**: the run's `status` is
`complete`, and there is no `callgraph_run_frontier` row for it. This is
the key distinction to keep straight when reading a run's result - a
truncated run stopped because of a budget it could have kept going past;
a complete run that hit an external boundary stopped because there was
genuinely nothing further to traverse into.

## The frontier: structural budgets vs. time/cancellation

`callgraph_run_frontier(run_id, symbol_id, reason)` records discovered-but-
not-admitted-or-expanded endpoints. What ends up there differs by the kind
of stop:

- **Structural budgets** (`max_depth`, `max_nodes`, `max_edges`): the
  frontier holds the discovered endpoints that were not admitted or
  expanded - for example, a `--max-depth 1` run's frontier held the single
  depth-1 node (`(anonymous namespace)::totalArea`) whose expansion the
  depth cap prevented, with `reason='max_depth'`.
- **Time limits and cancellation**: the frontier additionally includes the
  admitted node whose expansion was actually in progress when the stop
  happened, plus every remaining eligible selected root that had not yet
  been reached.

These budget-and-cancellation frontier semantics apply uniformly to
`callees`, reverse `callers`, and path (`--to`) queries.

## Runtime callees vs. what the graph proves

`DispatchCalls` (see [Overview](01-overview.md#virtual-dispatch-dispatchcalls))
is a conservative over-approximation of what a virtual call could invoke -
it can list targets that are never actually reached for a given object's
real dynamic type. An indirect or otherwise unresolved call target has **no
destination row at all**: it is recorded separately in
`callgraph_unresolved_site(source_id, file_id, offset, line, col)`, with no
destination field, because the tool cannot invent an external target for a
call it cannot resolve.

Put together, a call-graph run answers "what does the stored evidence show
this traversal reaching," never "what will actually execute at runtime for
a given input." Treat `DispatchCalls` edges as candidates to investigate
further (for example, by checking the object's static/dynamic type at the
call site) rather than as proof that every listed override actually runs.
