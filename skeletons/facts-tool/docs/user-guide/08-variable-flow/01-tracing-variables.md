# Tracing local variables and parameters

← [User guide index](../README.md) · [Table of contents](../toc.md)

`analyse variable-flow` follows one local variable or parameter through its
function, calls, assignments, returns and control-flow paths. It writes a
standalone SQLite artifact and records source locations for every retained
occurrence.

First import the project's compilation database, as described in
[Importing compile commands](../02-projects-and-configuration/02-importing-compile-commands.md),
then select a function and variable:

```sh
facts-tool analyse variable-flow --function 'example::process' --variable value
```

Select an overloaded function by its qualified name and parameter types:

```sh
facts-tool analyse variable-flow --function 'example::process(bool, int)' --variable value
facts-tool analyse variable-flow --function 'example::Worker::process(int) const &' --variable value
```

Quote the entire selector. Include parameter types, without parameter names,
default arguments, or a return type; use `()` for zero parameters. Member
function `const`, `volatile`, `&`, and `&&` qualifiers distinguish overloads.
Whitespace between signature tokens is ignored. If a selector is ambiguous,
the diagnostic lists candidate signatures and exact USRs to choose from.
Signatures match Clang's declared or canonical type spellings; copy a listed
signature when an equivalent C++ spelling does not match.

Existing qualified function names and exact Clang USRs remain supported.
Variable selectors accept a name or USR. Use `--line` when a variable name is
shadowed or ambiguous; parameters use the same selection rules. `--conf`,
`--config`, `--extra-arg` and source selectors follow the existing command
configuration rules.

The default traversal has no call-depth cap. `--max-depth 0` keeps the graph
inside the selected function; a positive value expands calls up to that depth
and records a `depth-limit` boundary at the next call.

```sh
facts-tool analyse variable-flow --function 'example::process' --variable value --max-depth 2
facts-tool analyse variable-flow --function 'example::process' --variable value --max-depth 0
```

Recursive calls terminate because the graph keeps one context-insensitive
summary for each function. Repeated call sites share that function's nodes and
summary facts, while argument, return and effect edges retain their originating
call site in `edge.callsite`.

By default all imported translation units are available. Source arguments
restrict the analysis input, so a definition outside that selection becomes an
explicit boundary.

## SQLite output

When YAML resolves a `facts_template`, the default artifact is that path with
its extension replaced by `.variable-flow.db`. Use `--output` for a different
path. An explicit output is also valid with only a project configuration and no
`facts_template`:

```sh
facts-tool analyse variable-flow --conf project.db \
  --function 'example::process' --variable value --output flow.db
```

When `facts_template` contains `{filename}` or `{relative_path}`, it resolves
one facts input per source; pass `--output` for an analysis spanning multiple
translation units.

The command validates selectors and all source inputs before opening the
artifact, appends each committed graph as a new run, and prints its ID only
after commit:

```text
facts-tool: variable flow run 1 complete
```

The output cannot alias the project configuration, source files or configured
facts database. Existing facts schemas are never rewritten.

The public Python reader is read-only:

```python
from facts_tool import open_variable_flow

with open_variable_flow("flow.db") as flows:
    run = flows.get(1)  # Use the run ID printed by the command.
graph = run.graph
for node in graph.reads(variable_usr=run.root_variable):
    print(node.kind, node.location.file, node.location.line)
for node in graph.writes(variable_usr=run.root_variable):
    print(node.kind, node.location.file, node.location.line)
print(run.status, run.boundaries)
```

Updates appear in both `reads()` and `writes()`. Omit the variable filter to
inspect accesses across the entire retained dependency slice, including
callee parameters and copied values. `graph.incoming(node_id)` and
`graph.outgoing(node_id)` expose adjacent edges; resolve their endpoints with
`graph.node(edge.source)` and `graph.node(edge.target)`. Nonzero
`edge.callsite` values identify the originating call occurrence.

See the [Python variable-flow API](../../../python/docs/variable-flow.md) for
node filters, boundary queries, immutable results, and reader errors. The
reader does not execute analysis or refresh a graph when sources change;
generate a new native run when current-source evidence is required.

## Graph semantics

Nodes identify functions, CFG blocks, parameters, calls, reads, writes,
updates and returns. `contains` connects a function to its CFG blocks and a
block to its occurrences. `control` connects CFG blocks. `data` connects a
reaching definition to a read or update. `value` records an expression or
initializer dependency. `call`, `return` and `call-result` connect calls to
function summaries and returned values. `argument-copy` represents a value
parameter, while `argument-ref` represents a reference argument sourced from
the caller's read and `argument-pointer` represents a pointer argument.
`argument-pointee` connects a caller memory read to a callee pointee parameter
when pointed storage is read.
`effect` connects a call to its caller-side synthetic write; a callee write or
update reaches that synthetic write through `reference-effect`. `unknown-effect`
records a possible indirect storage change.

The reaching analysis is path conservative: a join can retain writes from both
branches, and a loop update can reach the update on its next iteration. A write
kills earlier definitions in that block; an update reads its incoming
definitions before replacing them. A value-copy parameter has no caller-side
effect edge.

Pointer storage and a pointer variable are separate identities. Memory-effect
nodes use the variable USR suffix `#pointee`, so rebinding a pointer does not
kill the pointed storage definition. The analysis follows known simple aliases,
such as a direct reference initialized from a variable. More complex or
ambiguous aliases receive an explicit boundary.

External and depth-limited calls are normal completed results with their
boundary records. Indirect, virtual, unsupported, unmapped-CFG and ambiguous
alias boundaries make the run `partial`; the boundary explains which part of
the dependency could not be modeled. A partial graph is still immutable and
readable, but it does not claim complete coverage.

This analysis is opt-in and does not change `extract`, `match`,
`analyse call-graph` or existing Python facts queries.
