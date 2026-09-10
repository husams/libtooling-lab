# Importing compile commands

`import` is how compile commands - the exact compiler, flags, and working
directory used to build each source file - get into the project database.
Every later `extract`, `match`, or `analyse` run against a source file
depends on that file having a stored compile command from a prior `import`.

## Two ways to supply compile commands

```console
$ facts-tool import --help
Import compile commands into a project configuration

facts-tool import [OPTIONS] [sources...]

POSITIONALS:
  sources TEXT ...            Source files to import; filter compilation database commands or
                              use --extra-arg arguments

OPTIONS:
  -h, --help
  -v, --verbose LEVEL:INT in [0-3] [1]
  -f, --facts FILE            Existing facts database whose entries must be invalidated before
                              a project mutation
  -c, --conf FILE             direct project DB
      --config FILE           YAML defaults
  -p, --compilation-database DIR:DIR   Directory containing compile_commands.json
      --component NAME=PATH   Project component as name=path; repeatable
      --extra-arg ARG         Compiler argument replacing YAML extra_args for fixed-command or
                              compile_commands.json imports; shell-tokenized and repeatable
```

**With `-p`/`--compilation-database DIR`**, `import` reads a standard
`compile_commands.json` from that directory and imports every command it
contains (optionally filtered to just the `sources` you list). This is the
normal path for any project with a real build system, or for a
hand-written `compile_commands.json` like the one built in
[04-quick-start](../01-introduction/04-quick-start.md).

**Without `-p`**, `import` instead builds a `FixedCompilationDatabase` from
the `sources` positional arguments plus `--extra-arg`/the resolved YAML
`extra_args` - no JSON file needed at all. This is useful for a single
ad-hoc source you want indexed without setting up a build system.

## A verified run

```console
$ facts-tool import -c demo.db -p proj -v 1
facts-tool: import: starting
facts-tool: import: parse components
facts-tool: import: load compilation database
facts-tool: import: open project database
facts-tool: import: read file registry
facts-tool: import: store compile commands
facts-tool: import: register files
[1/2] Processing file .../proj/src/shapes.cpp.
[2/2] Processing file .../proj/src/main.cpp.
facts-tool: import: complete
Imported 2 compile command(s)
Registered 820 file(s)
```

## Why "820 file(s)" for a 2-source project

`import` discovers and registers every file transitively `#include`d by
each imported source, not just the sources themselves - system headers,
libc++ headers, and SDK headers all become their own catalog `file` rows
with an owning directory. This is intentional: it's the basis for
cross-translation-unit symbol joining, and it's why `dir list` after a real
import shows dozens of `external`-kind directories under paths like
`Library/Developer/CommandLineTools/SDKs/...` or
`opt/homebrew/Cellar/llvm/.../include/c++/v1/...`. A first-time reader
seeing a `dir list`/`file list` dominated by compiler and SDK paths is
seeing expected behavior, not a misconfiguration.

## `--component NAME=PATH`

Repeatable; associates a name with a path for this import. As covered in
[01-repositories-and-projects](01-repositories-and-projects.md#known-issues-dont-pre-register-or-alias-a-component-before-import),
this does **not** reuse an existing component of that name - it creates a
new one - so combining it with a prior `component add` of the same name
produces an orphan, ambiguous component row. Prefer the plain `import -p
DIR` workflow with no `--component` flag unless you specifically need to
split one compilation database across multiple named components.

## Re-importing an existing project

`-f`/`--facts` on `import` exists specifically for **re-import** of an
existing, non-empty project: an existing non-empty project without an
explicit facts path or facts template rejects mutations with an actionable
error, while initial empty-project setup remains allowed. In practice, if
you're updating an already-imported project's compile commands (for
example after adding a new source file), pass `-f`/`--facts` pointing at
the paired facts database so `import` can correctly invalidate any
call-graph entries that depended on the old configuration. See
[04-call-graphs/04-recovery-and-boundaries](../04-call-graphs/04-recovery-and-boundaries.md)
for what invalidation means for previously-generated call-graph data.

## `--extra-arg`

Repeatable, shell-tokenized compiler arguments. CLI `--extra-arg` values
replace matching YAML compiler options for this invocation, preserving
unrelated defaults without modifying the YAML file. See
[03-configuration-files](03-configuration-files.md) for the full precedence
model this participates in.

## Platform flags are handled for you

Neither the worked example's `compile_commands.json` nor its individual
compiler invocations need `-isysroot` or `-resource-dir` entries:
`facts-tool` injects the resource directory (from the linked libClang) and
the SDK path (from `xcrun --show-sdk-path` on macOS) automatically whenever
a stored command lacks them. This was confirmed by successful `extract` and
`analyse dependency` runs against a `compile_commands.json` containing zero
platform flags.

## What's next

- [03-configuration-files](03-configuration-files.md) covers where
  `import` (and every other command) resolves its default database paths
  from when you don't pass `-c`/`-o`/`-f` explicitly.
- [03-extracting-facts/01-extract](../03-extracting-facts/01-extract.md)
  covers what happens once compile commands are in place and you run
  `extract`.
