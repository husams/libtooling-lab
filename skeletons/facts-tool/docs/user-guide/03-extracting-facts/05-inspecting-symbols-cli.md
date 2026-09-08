# Inspecting Symbols, Files, and Directories from the CLI

Once a project has been imported and extracted, three command groups let
you inspect what was found without writing any Python: `symbol` reads the
facts database (plus, optionally, the project database for extra columns);
`file` and `dir` read the project database's catalog. This chapter covers
every leaf of all three groups.

All three groups share the same configuration flags as every other
`facts-tool` command (`-c/--conf`, `--config`, `-v/--verbose`), and all
three also take a group-level `-f/--facts FILE`. The flag means different
things in each group: for `symbol` it names the facts database to read
(`Extracted facts database; defaults to facts_template when omitted`), while
for `file` and `dir` it names the facts database whose call-graph entries
are invalidated before a mutation (see
[Extracting Facts](01-extract.md)).

## `symbol`

```text
facts-tool symbol --help

SUBCOMMANDS:
  list, ls      List extracted symbols
  show          Show symbols by exact qualified name
  browser       Browse extracted symbols interactively
  find          Find matched symbol candidates
  index         Manage matched-symbol index
```

`list`, `show`, `browser`, and `find` all take `-f/--facts FILE` (defaults
to `facts_template`) **and** an optional `-c/--conf FILE` - supplying
`--conf` enables the full source-path and repository/component join columns
in the output.

### `symbol list`

Lists every extracted symbol. Real output (head of an 83-symbol facts
database):

```console
$ facts-tool symbol list -f demo-facts.db
kind                qualified name
namespace           (anonymous)
function            (anonymous namespace)::totalArea(const std::vector& items) -> double
function            main() -> int
class               <lambda@25:41>
instance-method     main()::<lambda@25:41>::operator()(double value) const -> double
constructor         main()::(lambda)::(lambda at .../main.cpp:25:41)
...
class               std::exception
...
```

### `symbol show QUALIFIED-NAME`

Shows the full record for one exact qualified name. With `--conf`, the
output includes the full source path:

```console
$ facts-tool symbol show 'shapes::Circle::area' -c demo.db -f demo-facts.db
shapes::Circle::area() const -> double
  identity   820:7
  kind       instance-method
  type       Function
  language   C++
  access     public
  source     shapes.hpp:17:10
             .../proj/include/shapes.hpp
  usr        c:@N@shapes@S@Circle@F@area#1
  properties none
  flags      definition, virtual, const, override
```

`identity 820:7` is `<file_id>:<index>` - the same packed identity scheme
the Python SDK's `SymbolId` uses (see
[opening databases](../05-python-sdk/02-opening-databases.md)).

### `symbol browser`

An interactive terminal browser for extracted symbols (`Browse extracted
symbols interactively`). It has no documented non-interactive mode or
`--format` flag - use `symbol list`/`show` in scripts and automation
instead.

### `symbol find`

Looks up candidates in the [matched-symbol index](03-match-dynamic-matchers.md#the-matched-symbol-index),
which is populated only by `match`:

```text
facts-tool symbol find [OPTIONS]

OPTIONS:
          --kind INT          Raw Clang index symbol kind
          --format TEXT:{text,json}
                              Output: text or json
[Option Group: selector]
  Select exactly one
  [Exactly 1 of the following options are required]

OPTIONS:
          --usr TEXT          Exact USR
          --name TEXT         Literal qualified-name substring
```

```console
$ facts-tool symbol find -c demo.db --name area
USR                            QUALIFIED NAME        FILE ID  KIND  PATH                         COMPONENT  REPOSITORY
c:@N@shapes@S@Circle@F@area#1  shapes::Circle::area  2        17    .../proj/src/shapes.cpp      proj       proj
c:@N@shapes@S@Circle@F@area#1  shapes::Circle::area  820      17    .../proj/include/shapes.hpp  proj       proj
INDEX SCOPE: matched-only
SOURCE COMPLETE: unknown
```

Rows are tab-separated on the wire; the columns are aligned here for
readability, and the `PATH` values are absolute in real output.

`--name` is a case-sensitive literal substring match, not a glob - `%` and
`_` are ordinary characters. An empty result means only that no successful
match recorded that candidate; it never proves the symbol is absent.

### `symbol index clear`

```console
$ facts-tool symbol index clear -c demo.db --file-id 2
```

Clears matched-symbol candidates for one file ID; a positive but unknown
file ID is a successful no-op. See
[Matching with Dynamic Matchers](03-match-dynamic-matchers.md#clearing-the-index)
for the full behavior and a before/after example.

## `file`

```text
facts-tool file --help

SUBCOMMANDS:
  list, ls                    List registered files
  show                        Show one registered file
  add                         Register one source file
  rm, remove                  Remove one catalog file
  set-option                  Replace one exact option sequence
  clear-option                Remove one exact option sequence
```

`show`, `add`, and `rm`/`remove` each take one required `path` positional.

### `file list` / `file show`

`file list` after `import`, showing the project's own two sources followed by
the external include closure (output trimmed; the real rows are
tab-separated with absolute paths):

```console
$ facts-tool file list -c demo.db
ID  COMPONENT  DIRECTORY                                        FILE                     OVERRIDDEN  INDEXED  PATH
1   proj       src                                              main.cpp                 false       false    .../proj/src/main.cpp
2   proj       src                                              shapes.cpp               false       false    .../proj/src/shapes.cpp
3   external   Library/.../MacOSX26.1.sdk/usr/include            Availability.h           false       false    /Library/.../usr/include/Availability.h
4   external   Library/.../MacOSX26.1.sdk/usr/include            AvailabilityInternal.h   false       false    /Library/.../usr/include/AvailabilityInternal.h
...
```

`file show PATH` prints the per-file record for one path (not independently
exercised while writing this chapter; the columns match `file list`'s row
shape).

### `file add`

```text
facts-tool file add [OPTIONS] path

OPTIONS:
      --driver TEXT REQUIRED          Compiler driver executable
      --working-directory TEXT        Compilation working directory
      --arg TOKEN                     Exact compile argument; repeatable
```

Use this to hand-register a single file with a fixed compiler command. For
a project already described by a `compile_commands.json`,
[`import`](../02-projects-and-configuration/02-importing-compile-commands.md)
supersedes this - `file add` is for the one-off case where no compilation
database exists for that file.

### `file rm` / `file remove`

```text
facts-tool file rm [OPTIONS] path
```

Removes one catalog file. Removing a file also cascades its
[matched-symbol index](03-match-dynamic-matchers.md#the-matched-symbol-index)
candidates.

### `file set-option` / `file clear-option`

```text
facts-tool file set-option [OPTIONS]
facts-tool file clear-option [OPTIONS]

OPTIONS:
      --match TEXT REQUIRED   Case-sensitive ECMAScript regular expression
      --arg TOKEN REQUIRED    Exact contiguous argument token; repeatable
```

These edit stored compile options for every registered file whose
forward-slash-normalized path (relative to its component root) matches the
given ECMAScript `regex_search` pattern. Matching is on exact, contiguous
argument tokens - not substrings inside a single token - and an invalid or
unmatched expression rolls back the whole operation rather than partially
applying. These leaves mutate stored compile options, so treat them like
any other catalog mutation: pass `-f/--facts` if you have a paired facts
database whose entries should be invalidated first (see
[Extracting Facts](01-extract.md#how-catalog-mutations-invalidate-call-graph-entries)).

## `dir`

```text
facts-tool dir --help

SUBCOMMANDS:
  list, ls   List directories
  rm         Remove one directory and its catalog files
```

### `dir list`

```text
facts-tool dir list [OPTIONS]
      --component TEXT
```

After `import`, this shows one row per registered directory - the project's
own source directories plus every external `#include` directory the
compilation closure touched:

```console
$ facts-tool dir list -c demo.db
ID  COMPONENT  FILES  PATH
1   proj       2      src
2   external   40     Library/Developer/CommandLineTools/SDKs/MacOSX26.1.sdk/usr/include
3   external   10     Library/Developer/CommandLineTools/SDKs/MacOSX26.1.sdk/usr/include/_types
4   external   8      Library/Developer/CommandLineTools/SDKs/MacOSX26.1.sdk/usr/include/arm
...
```

The `PATH` column is the directory's path relative to its component root, so
the single project row reads `src` while the external rows carry their full
SDK-relative paths.

Expect this list to be dominated by `external`-kind rows under your
compiler's resource/SDK directories on a real project - `import` registers
every transitively included header as a catalog file, so a two-source,
two-header demo project produced **820 registered files** once its
`<memory>`, `<functional>`, `<vector>`, `<iostream>`, and libc++/SDK closure
were counted. This is expected behavior (symbols are joined by canonical
USR across the whole closure), not a defect - don't be alarmed by a
`dir list`/`file list` full of Homebrew LLVM or Xcode SDK paths.

### `dir rm`

```text
facts-tool dir rm [OPTIONS]
          --component TEXT
          --dry-run
[Option Group: selector]
  Select exactly one object
  [Exactly 1 of the following options are required]

OPTIONS:
          --id INT:POSITIVE
          --path TEXT
```

Removes one directory and its catalog files by ID or path, optionally
scoped to one component. `--dry-run` reports what would be removed without
mutating anything.
