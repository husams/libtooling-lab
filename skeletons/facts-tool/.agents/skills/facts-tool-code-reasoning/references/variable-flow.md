# Track a local variable or parameter

Use this workflow for a variable's reads, writes, updates, copied values,
arguments passed into callees, or values returned by called functions.
The native command performs the Clang AST/CFG and interprocedural analysis;
the Python SDK queries its saved graph. Do not recreate the analysis in
Python or query SQLite directly.

## Select the declaration and input scope

Check the selected executable and Python environment first:

```sh
facts-tool config show
facts-tool analyse variable-flow --help
python -c 'from facts_tool import open_variable_flow, VariableFlowGraph'
```

If using IPython-MCP, run the Python import in its existing runtime instead.
An older installed executable or wheel may lack these capabilities even when
checkout documentation describes them; report or repair the actual gap within
the authorized task.

Use `symbol find` for an existing function identity, or a scoped native matcher
when discovery evidence is missing. An absent index entry does not establish
that a local variable is absent. The flow command resolves the local or
parameter declaration within the selected function from the AST.

Import real compilation commands if the project is not registered:

```sh
facts-tool import -p build
facts-tool analyse variable-flow --function 'example::process' --variable value
```

Replace the example selectors with the requested function and variable.
For an overload, include the function's parameter types in the quoted selector:

```sh
facts-tool analyse variable-flow --function 'example::process(bool, int)' --variable value
facts-tool analyse variable-flow --function 'example::Worker::process(int) const &' --variable value
```

Use types without parameter names, default arguments, or a return type; `()`
selects zero parameters. Include member `const`/`volatile` and `&`/`&&`
qualifiers where applicable. Whitespace between signature tokens is ignored.
An ambiguity diagnostic lists candidate signatures and USRs; use one of those
exact selectors instead of guessing which overload was selected. Qualified
names without signatures and exact USRs remain supported.
Matching uses Clang's declared or canonical type spellings; copy a candidate
signature when an equivalent C++ spelling does not match.

The variable accepts a name or exact USR; parameters use the same `--variable`
selector. Use `--line DECLARATION_LINE` to distinguish shadowed locals; a
use-site line does not select the declaration.

The command parses the imported source inputs; a prior `extract` or
`analyse call-graph` run is not required. Omit source arguments to make all
imported translation units available. Supplying sources restricts available
definitions, so include the caller and relevant callee TUs together.

Default depth is unlimited. Add `--max-depth 0` for only the selected function,
or a positive limit when requested. Recursion terminates using shared function
summaries; unlimited depth does not enumerate every runtime call stack.

## Save and open the exact run

YAML resolves the project path and, when configured, the facts path. The
default flow artifact replaces the configured facts path's extension with
`.variable-flow.db`. It is a separate SQLite file: do not open it with
`open_codebase` or use the facts database as the flow output.

Use an intentional `--output flow.db` when choosing an artifact location, when
no `facts_template` exists, or when a per-source template would produce
multiple facts paths. Use `--config FILE` to select another YAML configuration.
Existing runs append; capture the run ID from a successful completion line:

```text
facts-tool: variable flow run 1 complete
```

The number is an example, not a fixed ID. A failed command publishes no new
successful run; do not silently read an older run after failure. A saved run
is a snapshot, not automatic freshness evidence for subsequently edited code.
Regenerate it when current-source analysis is required.

```python
from facts_tool import open_variable_flow

flow_path = "flow.db"  # Actual resolved artifact or intentional --output path.
run_id = 1  # Actual ID from this command's successful completion line.
with open_variable_flow(flow_path) as flows:
    run = flows.get(run_id)
graph = run.graph
print(run.status, run.max_depth, run.assumptions)
for access in graph.reads(variable_usr=run.root_variable):
    print(access.kind, access.location.file, access.location.line)
for access in graph.writes(variable_usr=run.root_variable):
    print(access.kind, access.location.file, access.location.line)
```

The loaded run is immutable and remains usable after closing the reader.
`reads()` and `writes()` both include updates. A possible call-side effect is
not proof that a write executes on every path. Filter by exact `variable_usr`
and optionally `function_usr`; repeated names in other scopes are distinct.
Omit filters to inspect the whole retained dependency slice, including callee
parameters and derived local values. That slice does not include unrelated
statements simply because they share a function.

## Explain the links and limits

Use `graph.incoming(node_id)` and `graph.outgoing(node_id)` for adjacent edges,
then `graph.node(edge.source)` and `graph.node(edge.target)` for occurrences.
Preserve `edge.kind` and `edge.callsite`; a nonzero callsite identifies the
originating call node. Shared callee summaries do not provide separate nodes
for every invocation. Node IDs are local to a run; never join different runs
by a bare node ID. Node-object arguments must come from that loaded run.

- `data` links reaching definitions to reads or updates; `value` links
  expression dependencies. Branch joins and loops can retain multiple writers.
- `argument-copy` passes a value without implying mutation of the caller.
  `argument-ref`, `argument-pointer`, and `argument-pointee` describe reference
  or pointer flow. `reference-effect` and `effect` connect callee writes to
  caller-side effects. Pointer binding and `#pointee` storage are distinct.
- `return` and `call-result` trace the production of a captured return value.
  Follow the stored edges, including local copies, rather than matching names.
- Inspect `run.boundaries` and `run.status` before drawing conclusions.
  External/unavailable definitions and `depth-limit` can accompany `complete`;
  this does not prove complete external behavior or unrestricted depth.
  Indirect calls, virtual dispatch, unsupported syntax, and ambiguous aliases
  can make the run `partial`. Report their reason and location instead of
  interpreting a missing edge as proof of no read or write.

Report the selected declaration, source locations of relevant accesses,
call-site evidence, run ID, input scope, and any limiting boundaries. Keep the
answer focused on the requested variable; do not dump the entire graph.
