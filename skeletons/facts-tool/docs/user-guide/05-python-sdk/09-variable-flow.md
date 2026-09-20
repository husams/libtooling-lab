# Local-variable and parameter flow

← [User guide index](../README.md) · [Table of contents](../toc.md)

Generate a graph with native `facts-tool analyse variable-flow`, then read the
exact saved run through `facts_tool.open_variable_flow`. The graph lives in a
standalone artifact; it does not require a `CodeBase` or a facts/project pair
to read. See [Tracing local variables and parameters](../08-variable-flow/01-tracing-variables.md)
for selectors, input scope, output paths, and native analysis semantics.

Select overloads in the native command with a quoted signature such as
`--function 'example::process(bool, int)'`. The reader preserves that text in
`run.function_selector` and the resolved overload's USR in `run.root_function`.

```python
from facts_tool import open_variable_flow

with open_variable_flow("flow.db") as flows:
    run = flows.get(1)  # Use the successful native command's actual run ID.
graph = run.graph
for read in graph.reads(variable_usr=run.root_variable):
    writers = graph.incoming(read.id, kind="data")
    print(read.location, [graph.node(edge.source) for edge in writers])
```

`graph.reads()` and `graph.writes()` include updates. Exact variable and
function USR filters distinguish shadowed variables and callee parameters;
omitting filters examines the whole retained dependency slice. Queries return
immutable saved facts and preserve each edge's callsite.

Inspect `run.status`, `run.assumptions`, `run.max_depth`, and `run.boundaries`
before claiming coverage. An external or depth-limited boundary may coexist
with a `complete` run. Unsupported behavior can make the run `partial`.
The SDK does not rerun Clang or prove freshness against edited source files.

The [Python API reference](../../../python/docs/variable-flow.md) documents
all graph queries, lifecycle behavior, errors, and runnable examples.
