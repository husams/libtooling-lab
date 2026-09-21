# Track a local variable or parameter through Python

Use `client.variable_flow` for reads, writes, updates, copied values,
arguments, and captured returns. The server performs AST/CFG and interprocedural
analysis; do not recreate it in Python or query its database.

## Select the function and declaration

Find the enclosing function through [global symbols](how-to-search-symbol.md).
Use an exact returned symbol ID or USR to resolve overloads; the function
`SymbolReference` also accepts an unambiguous exact qualified name.
Use a scoped matcher when the variable's declaration location is needed.

Inside an open client context:

```python
from facts_tool.rest import DeclarationLocation, SymbolReference, VariableReference

job = client.variable_flow.create(
    function=SymbolReference(qualified_name="example::Service::run"),
    variable=VariableReference(
        name="request",
        declaration=DeclarationLocation(
            path="src/Service.cpp", line=42, column=9,
        ),
    ),
    direction="forward",
    interprocedural=True,
)
summary = job.wait(timeout=120)
print(job.id, summary.root_function, summary.root_variable,
      summary.status, summary.coverage)
```

Replace the example coordinates with the actual declaration, not a use site.
A declaration location distinguishes shadowed locals; omit it for an
unambiguous name. Parameters use the same `VariableReference`.
The REST variable model takes a name and optional declaration location;
do not copy the CLI's signature-selector or `--line` syntax into the request.

Use real registered compiler commands. Variable flow parses source and does
not require a previous callgraph run. Optional `selection=` restricts source
inputs; include relevant caller/callee TUs together so definitions remain
available. Omit selection when all imported inputs should be available.

Only forward tracking is supported; do not submit backward requests.
Interprocedural tracking is enabled by default, with no requested call-depth
cap. Set `interprocedural=False` to stay in the starting function, or
`max_call_depth=N` for an intended bound. Recursion uses shared summaries,
not an enumeration of every runtime call stack.

## Read the exact job's evidence

```python
for node in client.variable_flow.nodes(job.id):
    print(node.id, node.kind, node.function_usr, node.variable_usr,
          node.name, node.location.path, node.location.line)
for edge in client.variable_flow.edges(job.id):
    print(edge.source, edge.target, edge.kind, edge.callsite)
for boundary in client.variable_flow.boundaries(job.id):
    print(boundary.node, boundary.reason, boundary.detail, boundary.depth)
for diagnostic in client.variable_flow.diagnostics(job.id):
    print(diagnostic.severity, diagnostic.message)
```

These collections are lazy. Summary `nodes`/`edges`/`boundaries` fields
may be `None` until fetched; never interpret that as zero accesses.
Use `client.variable_flow.get(job_id)` for a retained run instead of opening
a local artifact. Keep node IDs within their job; never join runs by a bare
integer node ID.

Identify accesses by `variable_usr` and `function_usr`, not name alone.
Retain the full dependency slice when it includes callee parameters or copied
locals; filtering every node to the root variable can hide downstream flow.
For adjacency or labels, use the returned edge/node identities from that run.

## Explain flow and limits

Preserve node kinds, edge kinds, locations, and call-site links:

- `data` links reaching definitions to reads/updates; `value` links
  expression dependencies. Joins and loops can have multiple writers.
- `argument-copy` passes a value without proving mutation of the caller.
  `argument-ref`, `argument-pointer`, and `argument-pointee` describe
  reference/pointer flow. `reference-effect` and `effect` connect callee
  writes to caller-side effects; pointer binding and pointee storage differ.
- `return` and `call-result` describe captured return flow. A nonzero
  `callsite` points to the originating call node in that run; shared callee
  summaries do not create a separate graph for every invocation.
- External/unavailable definitions and depth limits remain boundaries even
  when analysis completes. Indirect calls, virtual dispatch, unsupported
  syntax, and uncertain aliases can produce partial evidence.

An update can both read and write. A possible call-side effect is not proof of
a write on every path. A missing edge is not proof of no access across an
unresolved boundary. Report the selected declaration, relevant access sites,
job ID, input scope, and any limits. Re-run when the stored job predates
relevant source changes.
