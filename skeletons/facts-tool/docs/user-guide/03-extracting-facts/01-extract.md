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
      --force         Re-extract sources whose recorded index state is still up to date
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
| 1 | stages - one line per pipeline stage (`starting`, `validate database paths`, ..., `complete`), including the `up_to_date=N stale=M` freshness summary |
| 2 | details - adds extra stage lines: `configuration=...`, `selected_sources=N`, and one `skip up-to-date source=<path>` line per source the freshness check skipped |
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
facts-tool: extract: check index freshness
facts-tool: extract: up_to_date=0 stale=2
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
facts-tool: extract: record index state
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

## Skipping up-to-date sources

After the first successful extraction of a source, `extract` records what it
did for that file, and every header it transitively included, in each
file's own `file` row: that it is indexed, when, into which facts database,
and at which git commit. A later `extract` run reads that recorded state
back and skips a translation unit entirely - no Clang parse, no
output-database writes for it - only when **every** file in its transitive
include set (the TU itself and every header it pulls in) individually
passes **all** of the following, checked in this order:

1. The file is marked indexed, and its recorded facts database is the exact
   path this run would write to.
2. Its recorded git commit matches the current `HEAD` of the git repository
   that tracks it. This is a repository-wide check, not a per-file one: any
   new commit in that repository - even one that never touches this file -
   moves `HEAD`, so it makes every one of the repository's tracked files
   stale at once. A file outside any git repository, or untracked within
   one, compares as no-commit on both sides; that counts as a match. A file
   that moved into or out of a repository does not.
3. Its current last-write time exactly matches the recorded one. Any
   difference - forward or backward - counts as changed; there is no "not
   newer than" allowance, so a source whose mtime moves into the future
   (a clock change, a restored backup) is stale exactly once, not
   permanently. `indexed_at` is recorded metadata only and plays no part in
   this comparison.

Editing only a header is enough to make every translation unit that
includes it stale, even though none of those TUs' own files changed.
Any other source - one whose closure fails any rule, or one `extract` has
never seen before - is stale and gets extracted normally.

> [!info]- What "skipped" means for included headers
> A source and everything it transitively includes are marked together at
> the end of a run that actually extracted it. When two facts databases
> extract overlapping sources that share a header, the header's recorded
> `facts_db` reflects whichever extraction ran **last** - the previous
> extraction's skip decision for that header is not retroactively affected,
> but a later run against the other facts database will see the header's
> `facts_db` pointing elsewhere and treat it as stale again.

If every requested source is up to date, `extract` prints one line and
exits 0 without opening or creating the output database and without
running the AST-extraction pass that would actually record facts.
Resolving registered sources still preprocesses every selected source
first regardless of the outcome - the `[n/m] Processing file` block always
prints, since that same pass is also how the freshness check discovers
each source's included headers - only the Clang parse and fact extraction
that would follow it is skipped:

```text
facts-tool: 2 source(s) up to date; nothing to extract
```

Pass `--force` to skip this check entirely and re-extract every requested
source regardless of its recorded state - useful after a toolchain change,
a manual edit to the facts database, or any time you do not trust the
recorded state:

```console
$ facts-tool extract -c demo.db -o demo-facts.db --force proj/src/shapes.cpp
```

> [!warning]- If the output database goes missing between runs
> The freshness check only compares recorded paths, not whether the output
> file still exists on disk. Deleting `demo-facts.db` and re-running
> `extract` without `--force` reports "up to date; nothing to extract" and
> does **not** recreate it - pass `--force`, or extract into a fresh path,
> whenever the previous output no longer exists.

Several other commands reset a file's recorded index state too, each
scoped to exactly what it invalidates rather than the whole project:

- `file set-option`/`clear-option` resets only the row(s) the match
  actually touched - other registered files are unaffected.
- `file add`/`rm` resets nothing: a newly added row is unindexed already,
  and a removed row is simply gone.
- `import` resets a row only when this import actually changed its driver,
  working directory, or compile options; a brand-new row is unindexed
  already either way. Re-importing an unchanged `compile_commands.json`
  therefore leaves every row's recorded index state alone, so the next
  plain `extract` still skips it - the everyday `import && extract`
  workflow is not defeated by a routine reimport.
- `repository`, `component`, `directory`, and `clone` mutations reset
  every row in the project catalog, since a structural change can move
  what an existing row even refers to. This coarse reset happens only when
  the mutation actually invalidated a paired facts database's call-graph
  entries; a project with no facts database configured for it yet has
  nothing to invalidate, and so nothing to reset either.

Whichever command triggers it, resetting index state re-arms the next
plain `extract` to re-extract rather than trust bookkeeping the mutation
just made obsolete.

The three new `file` columns this feature reads and writes -
`indexed_at`, `facts_db`, `git_commit` - are visible on
[`facts-tool file show`](05-inspecting-symbols-cli.md) and documented in the
[storage schema reference](../07-reference/02-storage-schema.md).

Recording index state is an optimization a later `extract` can use to skip
unchanged work, not a correctness requirement for the facts just committed:
by the time this step runs, the facts are already durably written. Every
failure recording it - the project database cannot be opened read-write
(most commonly because it is read-only), a file cannot be resolved or
stat'd, or the `UPDATE` itself fails, including a transient `SQLITE_BUSY`
from a concurrent reader - is therefore only ever a warning, never a reason
to fail a command that already did its real work; `extract` still exits 0:

```text
facts-tool: warning: index state not recorded: ...
```

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
