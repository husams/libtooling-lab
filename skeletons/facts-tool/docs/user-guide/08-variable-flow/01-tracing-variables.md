# Tracing local variables and parameters

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

The function and variable selectors accept a qualified name or a Clang USR.
Use `--line` when a variable name is shadowed or ambiguous; parameters use the
same selection rules. `--conf`, `--config`, `--extra-arg` and source selectors
follow the existing command configuration rules.

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
    print(run.nodes)
    print(run.edges)
    print(run.boundaries)
```

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
