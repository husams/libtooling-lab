# Extracting Facts

`facts-tool extract` walks the Clang AST of already-imported translation
units and writes symbols, relations, and call evidence into a facts
database. It never parses a file that has not first been registered by
[`facts-tool import`](../02-projects-and-configuration/02-importing-compile-commands.md) -
extract only reads stored compile commands, it does not discover sources on
its own.

```text
facts-tool extract [OPTIONS] [sources...]

POSITIONALS:
  sources TEXT ...   Source files to extract; defaults to all imported files

OPTIONS:
  -v, --verbose LEVEL:INT in [0-3] [1]
  -o, --output FILE   SQLite database for extracted facts; defaults to facts_template when omitted
  -c, --conf FILE     Direct project DB path (overrides FACTS_TOOL_CONF and generated naming)
      --config FILE   YAML defaults file
      --extra-arg ARG Compiler argument replacing YAML extra_args; shell-tokenized and repeatable
```

## Extracting all sources vs. selected sources

Pass one or more source paths to extract only those translation units:

```console
$ facts-tool extract -c demo.db -o demo-facts.db -v 2 proj/src/shapes.cpp proj/src/main.cpp
```

Omit the positional `sources` entirely to extract **every** file the project
database's compile-command registry knows about. In practice this means:
after `facts-tool import -p build` registers a compilation database, a plain

```console
$ facts-tool extract -c project.db
```

processes every registered translation unit in one pass and commits them all
into one shared facts database. A source path must be resolvable against the
project's compile-command registry; a file that was never imported cannot be
extracted directly.

Extracting with zero sources and no explicit `-o` still requires exactly one
resolvable output path. If the resolved `facts_template` contains no
per-source placeholder (`{relative_path}`/`{filename}`), every registered
source is written into that single shared database. A source-dependent
template (one that does contain those placeholders) requires selecting
exactly one source per invocation, since the template would otherwise
resolve to a different path per file within the same run.

## Choosing the output database

`-o`/`--output` is the facts database extract writes into. When omitted, the
path is resolved from the merged YAML `facts_template` (see
[configuration files](../02-projects-and-configuration/03-configuration-files.md)
for placeholder syntax and precedence). Run
[`facts-tool config show`](../02-projects-and-configuration/04-config-command.md)
before your first extraction on a machine - a pre-existing user-level
`~/.config/facts-tool/config.yaml` can silently redirect every generated
facts database under `~/.cache/facts/<project_name>/...` instead of next to
your checkout. `-o` always overrides the template explicitly.

## Verbosity levels

`-v`/`--verbose` accepts an integer `0`–`3` (default `1`):

| Level | Behavior |
|---|---|
| 0 | quiet - suppresses the `facts-tool: extract:` stage lines only |
| 1 | stages - one line per pipeline stage (`starting`, `validate database paths`, ..., `complete`) |
| 2 | details - adds two extra stage lines, `configuration=...` and `selected_sources=N` |
| 3 | trace - adds low-level trace detail (thousands of lines on a small project) |

Verbosity controls the `facts-tool: extract:` stage lines and nothing else.
Clang's own `[N/M] Processing file ...` progress lines, the
`coverage.unsupported_semantics` notices, and the final
`N symbol(s) recorded from M file(s)` summary print at **every** level,
including `-v 0`. All of this goes to standard error, not standard output;
`extract` writes nothing to standard output at all.

A worked run at `-v 2` against a 2-file, 2-header C++17 project (abstract
`Shape` with `Circle`/`Square`, a function template, and a lambda):

```console
$ facts-tool extract -c demo.db -o demo-facts.db -v 2 proj/src/shapes.cpp proj/src/main.cpp
facts-tool: extract: starting
facts-tool: extract: configuration='demo.db', output='demo-facts.db', requested_sources=2
facts-tool: extract: validate database paths
facts-tool: extract: load compilation database
facts-tool: extract: validate stored commands
facts-tool: extract: extract facts
facts-tool: extract: open project database
facts-tool: extract: validate registry completeness
facts-tool: extract: select sources
facts-tool: extract: selected_sources=2
facts-tool: extract: resolve registered sources
[1/2] Processing file .../proj/src/shapes.cpp.
[2/2] Processing file .../proj/src/main.cpp.
facts-tool: extract: configure Clang tool
facts-tool: extract: open output database
facts-tool: extract: begin output transaction
facts-tool: extract: Clang parse and AST extraction
[1/2] Processing file .../proj/src/shapes.cpp.
facts-tool: coverage.unsupported_semantics kind=implicit-cleanup site=.../c++/v1/__chrono/duration.h:340:8
... (12 more coverage.unsupported_semantics lines for shapes.cpp, all in libc++/Clang resource headers)
[2/2] Processing file .../proj/src/main.cpp.
... (18 more, including one at proj/src/main.cpp:25:41 - the lambda's implicit destructor)
facts-tool: extract: commit output transaction
facts-tool: 83 symbol(s) recorded from 24 file(s)
facts-tool: extract: complete
```

The sources are listed twice because the run resolves the registered source
set first and then hands the same list to the Clang tool; the second pass is
the one that actually parses.

`coverage.unsupported_semantics kind=implicit-cleanup site=...` lines are
emitted for every implicit destructor call the call-graph pass could not
attribute a precise call-site column for. Most of them point into libc++
internals (`duration.h`, `tuple`, `hash.h`, `no_destroy.h`), but the same
notice fires for project code too - in this run one line pointed directly at
the lambda `[](double value){ return shapes::doubled(value); }` at
`main.cpp:25:41`. This is routine noise on any real C++ translation unit that
includes the standard library. It does not affect the exit code or the
recorded symbol count, and it is not a failure to investigate. Note that
`-v 0` does **not** silence these notices; redirect standard error if you
need a genuinely quiet run.

## Exit codes

Every `facts-tool` command shares one exit-code contract:

| Exit | Meaning |
|---|---|
| 0 | success (including a call-graph run whose status is `truncated`) |
| 1 | runtime/database/operational error |
| 2 | usage error (bad flag, bad selector) |
| 3 | configuration error |
| 130 | cancelled by SIGINT |

Every error is prefixed `facts-tool: `. Exit 2 and exit 3 add a category
word after that prefix (`usage error:`, `configuration error:`); exit 1
errors carry no category and read as a bare message, for example:

```text
facts-tool: project configuration database not found: /path/to/nope.db
facts-tool: usage error: The following argument was not expected: --bogus
facts-tool: configuration error: cannot create facts_template directory: No such file or directory
```

A `--conf` (or resolved `conf_template`) path that does not exist is always
a database error (exit 1), never a configuration error.

## Re-extraction and incremental behavior

Extraction is not additive across unrelated content: re-running `extract`
against the same output database republishes the selected sources' evidence
inside one committed transaction. Per `docs/call-graph-entries.md`,
generation and entry publication share the facts transaction, so a failure
during commit restores the previously committed entry state rather than
leaving a half-written result. **Extract every source file you want fully
represented in the same invocation** - a library-only extraction, or an
extraction whose sources produce zero matching evidence, clears caller
entries for functions that call into the un-extracted sources; those callers
are only republished once you extract the sources together.

## How catalog mutations invalidate call-graph entries

`extract` itself does not take a `-f`/`--facts` flag - it always writes
straight to its own output. But `import` and every catalog command
(`repo`, `component`, `dir`, `file`) accept a group-level
`-f`/`--facts FILE`, because those commands mutate the **project**
database, and a project mutation (a changed compile command, a removed
file, an edited compile option) can invalidate previously committed
call-graph entries in a **paired facts database**:

```console
$ facts-tool import -c project.db -f facts.db -p build
```

Supplying `--facts` tells the mutating command which facts store to
invalidate before the project-side change commits; the invalidation and the
project mutation share one outcome - if invalidation fails, the mutation
does not commit either. An existing nonempty project without an explicit
facts path or facts template rejects mutations with an actionable error;
initial empty-project setup remains allowed. Listing and dry-run catalog
commands never invalidate anything.

After any invalidating mutation, entries stay invalid until you run
`extract` again on the affected sources - invalidation never fabricates or
retroactively repairs evidence, it only clears the previous claim so a stale
entry can never be mistaken for a fresh one. See
[entries, runs, and status](../04-call-graphs/03-entries-runs-and-status.md)
for exactly what a function entry represents and how `entry_available`
reads after invalidation.
