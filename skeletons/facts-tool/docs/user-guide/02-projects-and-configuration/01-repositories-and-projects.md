# Repositories and projects

The **project database** (also called the project/configuration database or
catalog) is where `facts-tool` records everything about a codebase that
isn't a Clang fact: which repositories exist, which checkout clones back
them, how the project is broken into components, which directories and
files were seen, and what compile options each file was built with. Every
extraction, match, and call-graph run reads or writes against a project
database paired with a facts database.

This chapter covers the catalog commands (`repo`, `component`, `dir`,
`file`) and the recommended way to set up a new project. It also documents
two real defects found while building this guide's worked examples, so you
can avoid the workflow that triggers them.

## The catalog hierarchy

The project database organizes a codebase into four levels:

- A **repository** (`repo`) is the top-level registration for a codebase -
  a name plus one or more **clones** (checkouts) of it, one of which is
  marked active at a time.
- A **component** is a named subdivision of a project - typically one
  checkout, one library, or one external dependency - with its own root
  path and its own set of compile commands.
- A **directory** (`dir`) is one indexed directory under a component.
- A **file** is one registered source or header, with its own compile
  options and indexing state.

You will use `repo` and (indirectly, through `import`) `component`
constantly; `dir` and `file` are mostly inspection surfaces you'll query
rather than hand-edit.

## `repo`: registering a codebase

```console
$ facts-tool repo add demo "$PROJ" -c demo.db
Repository registered
```

`repo add NAME PATH` registers a new repository; the name must be new and
the path must be an existing directory that isn't already a registered
clone of something else. `repo list` (or `repo ls`) prints one row per
repository with its ID, name, kind, component count, clone count, and
active clone:

```console
$ facts-tool repo list -c demo.db
ID  NAME  KIND  COMPONENTS  CLONES  ACTIVE CLONE
1   demo  repo  0           1       .../proj
```

`repo show NAME` prints the same row, then a full clone list marking the
active clone with `*`, then a nested component table:

```console
$ facts-tool repo show demo -c demo.db
ID  NAME  KIND  COMPONENTS  CLONES  ACTIVE CLONE
1   demo  repo  0           1       .../proj
Clones (* active):
* 1  active  .../proj
ID  NAME  KIND  VERSION  REPOSITORY  FILES  ROOT
```

The nested component table is empty here, and `COMPONENTS` reads `0`, even
after a successful `import`: the component `import` creates is not linked
back to the repository. That is the same root cause as the known issues
below.

Other `repo` leaves - `add-clone`, `switch`, `rm-clone`/`remove-clone`, and
`rm` - manage multiple clones of the same repository (for example, tracking
both a local checkout and a CI checkout of the same codebase) and removing
repositories; consult
[07-reference/01-cli-reference](../07-reference/01-cli-reference.md) for
their exact option lists.

A fresh project database always has one **built-in `facts-tool` component**
already present (`kind=external`, root `/`, id 1) before you run any `repo`
or `component` command:

```console
$ facts-tool component list -c demo.db
ID  NAME        KIND      VERSION  REPOSITORY  FILES  ROOT
1   facts-tool  external  -                    0      /
```

This is expected, not something you created; it exists so that
compiler-provided and other rootless facts have a component to belong to.
Note that this is also the last moment `component list` succeeds in the
recommended workflow, for the reason given under Known issues below.

## `component`, `dir`, and `file`: what `import` fills in for you

In the normal workflow (see below), you never call `component add`
yourself - `import -p DIR` creates a component automatically from the
compilation database's directory basename. `component list`/`ls` and
`component show NAME` let you inspect what got created; `component add`,
`set-version`, `compile-commands`, and `rm` exist for cases where you're
managing components by hand instead of through `import`.

`dir list`/`ls` shows one row per registered directory - after a real
import, this includes not just your project directory but every external
include directory transitively pulled in (Homebrew LLVM's resource
directory, the macOS SDK's headers, and so on). `file list`/`ls` shows one
row per registered file, with its owning component and directory, whether
it's been indexed, and its resolved path.

Both groups also take a group-level `-f`/`--facts FILE` option, because any
catalog write can invalidate previously-generated call-graph entries in a
paired facts database - see
[04-call-graphs/04-recovery-and-boundaries](../04-call-graphs/04-recovery-and-boundaries.md)
for what invalidation means in practice.

## Recommended workflow

The workflow that works cleanly end to end, verified against a real
three-file C++17 project with a virtual-dispatch hierarchy, a template
function, and a lambda:

```console
$ facts-tool repo add demo "$PROJ" -c demo.db
$ facts-tool import -c demo.db -p "$PROJ" -v 1
$ facts-tool extract -c demo.db -o demo-facts.db "$PROJ/src/shapes.cpp" "$PROJ/src/main.cpp"
```

That is: `repo add` for provenance and clone tracking, then a plain
`import -p DIR` with **no** manual `component add` call and **no**
`--component` flag on `import`. This is the sequence used throughout
[04-quick-start](../01-introduction/04-quick-start.md), and it is the one
this guide recommends as the default for a new project.

## Known issues: don't pre-register or alias a component before `import`

Two real, reproducible defects were found while validating this workflow.
Both stem from the same root cause - `import -p DIR` always creates its own
component from the compilation database's directory basename unless told
otherwise - and both are avoided entirely by the recommended workflow
above. They are documented here so you recognize the symptom if you
deviate from it; see
[07-reference/04-limitations-and-known-issues](../07-reference/04-limitations-and-known-issues.md)
for the consolidated list across the whole guide.

**Issue 1 - pre-registering a component before `import` creates a
duplicate.** Running `component add --path PROJ --name demo --repo demo
--kind repo` and then `import -p PROJ` (without `--component`) registers
the checkout under both the manual component name and the automatically
created one, and a later `extract` on the imported sources fails:

```text
facts-tool: ambiguous stored compile commands for requested source '.../shapes.cpp'
```

**Issue 2 - `import --component NAME=PATH` does not reuse an existing
component of that name; it creates another one.** Whether or not you
called `component add` first, `component list`/`component show` then fail
on the resulting orphan row:

```text
facts-tool: component has no active clone: demo
```

or, if a manual `component add` ran first:

```text
facts-tool: ambiguous component
```

**Even the recommended workflow leaves one cosmetic wrinkle.** After a
plain `import -p DIR` with no manual component step, exactly one component
exists (auto-created, `kind=repo`, no `repo_id` link back to the
repository), and `component list`/`component show` still fail on it with
the same "no active clone" error shown above, exiting 1:

```console
$ facts-tool component list -c demo.db
facts-tool: component has no active clone: proj
$ echo $?
1
```

This does **not** block anything else - `dir list`, `file list`, `extract`,
`symbol`, `match`, and `analyse call-graph` all work normally against a
project set up this way. If you run `component list` right after a plain
`import -p DIR`, expect this error; it does not mean your import is broken.

## What's next

- [02-importing-compile-commands](02-importing-compile-commands.md) covers
  `import` itself in full: compilation-database mode vs. fixed-source mode,
  `--component`, `--extra-arg`, and re-import semantics.
- [03-configuration-files](03-configuration-files.md) and
  [04-config-command](04-config-command.md) cover how database paths get
  resolved when you don't pass `-c`/`-o`/`-f` explicitly.
