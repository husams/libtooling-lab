# Variable-flow runs

The variable-flow reader opens the standalone artifact produced by
`facts-tool analyse variable-flow`; it is read-only and never invokes the
native executable.

For an overloaded function, generate the run with a quoted native selector such
as `--function 'example::process(bool, int)'`. `run.function_selector` preserves
that input, while `run.root_function` is the selected function's exact USR.

```python
from facts_tool import open_variable_flow

with open_variable_flow("facts.variable-flow.db") as flows:
    run = flows.get(1)
    value = run.graph
    writes = value.writes(variable_usr=run.root_variable)
    for write in writes:
        for edge in value.outgoing(write):
            print(write.name, edge.kind, edge.target, edge.callsite)
```

`run.nodes`, `run.edges`, and `run.boundaries` remain available as immutable
tuples.  `run.graph.nodes(...)` filters by exact kind, name, function USR,
variable USR, or node ID. `reads(...)` includes `read` and `update` nodes;
`writes(...)` includes `write` and `update` nodes.  Passing an exact
`variable_usr` and optional `function_usr` to `nodes(...)` selects local or
parameter occurrences.

`incoming(...)` and `outgoing(...)` return `Edge` objects, so a callsite is
available as `edge.callsite`; resolve endpoint IDs with `graph.node(id)`.
`boundaries(...)` filters the explicit boundary records by node and reason.
Unknown node IDs raise `FactsToolError` with code `E_SOURCE`.
Node arguments to edge and boundary queries must come from the same loaded run;
use an integer ID for an explicit lookup within that run.

Use the reader as a context manager; `runs()` and `get()` after `close()` raise
`E_SOURCE`, as do missing run IDs and read failures while decoding graph rows.
