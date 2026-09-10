# Workflow: Large Codebases and Batching

## Goal

You need to extract facts from a codebase too large to comfortably run as
one `extract` invocation, and you need every teammate's (and CI's)
compiler flags and database paths to resolve identically. This chapter
covers `facts-tool-batch` at scale, and the configuration hygiene that
makes those large runs reproducible.

## Prerequisites

- A project database with the target sources already imported (see
  [Importing Compile Commands](../02-projects-and-configuration/02-importing-compile-commands.md)).
  `facts-tool-batch` never imports on its own.
- `facts-tool-batch` and its `facts_batch_*.py` helper modules installed on
  `PATH` (see
  [03-extracting-facts/04-batch-processing](../03-extracting-facts/04-batch-processing.md)).

## Steps

### 1. Don't rely on `-p` to filter the batch's source list: it unions

```console
$ facts-tool import -c project.sqlite -f facts.sqlite -p $FT/build file1.cpp file2.cpp   # ... (6 files)
Imported 6 compile command(s)
Registered 8349 file(s)
$ facts-tool-batch extract -j 4 -o out -c project.sqlite -p $FT/build file1.cpp   # ... (same 6 files)
FAIL .../CallSite.cpp (exit 1; ... facts-tool: requested source is not imported: ...)
FAIL .../sqlite3.c (exit 1; ...)
# ... 360 FAIL lines total
```

**What this tells you:** `-p` on `facts-tool-batch` enumerates every `file`
entry in the compilation database and **adds** it to your explicit source
list. It does not filter to the sources you named. A 6-file batch
combined with `-p` silently expands to the whole 360-entry database, and
every entry that was never imported fails with "requested source is not
imported."

### 2. Use `--files-from` to keep the batch scoped

```console
$ printf '%s\n' file1.cpp file2.cpp file3.cpp file4.cpp file5.cpp file6.cpp > files.txt
$ facts-tool import -c project.sqlite -f facts.sqlite -p $FT/build $(cat files.txt)
Imported 6 compile command(s)
Registered 8349 file(s)
$ time facts-tool-batch extract -j 4 -o out -c project.sqlite --files-from files.txt
facts-tool-batch: 6 succeeded, 0 failed
13.44s user 8.17s system 244% cpu 8.840 total
```

Six independent `<basename>-<sha256>.db` + `-extract.log` pairs land in
`out/`, one per source.

**What this tells you:** `--files-from` is the scoped alternative to `-p`.
The batch touches exactly the sources you listed. 244% CPU across `-j 4`
confirms real parallelism: each source runs in its own `facts-tool`
process, writing its own independent database.

### 3. Two batch runs against the same output directory don't race

```console
$ facts-tool-batch extract -j 2 -o out -c project.sqlite --files-from files.txt &   # run1
$ facts-tool-batch extract -j 2 -o out -c project.sqlite --files-from files.txt     # run2, started ~50ms later
facts-tool-batch: error: output directory is already locked: .../out
$ echo $?   # run2
1
# run1 finished normally: "facts-tool-batch: 6 succeeded, 0 failed", exit 0
```

**What this tells you:** the output directory is exclusively locked for
one invocation's duration. A second concurrent run against the same
directory fails fast rather than racing with or corrupting the first.
Schedule batches against distinct output directories, or serialize them.

### 4. Confirm the resolved configuration before running anything at scale

```console
$ cd $FT && facts-tool config show
project_root: "/Users/.../libtooling-lab"     # git root above cwd, NOT $FT
conf: "/Users/husam/.cache/facts/libtooling-lab/project.db"
conf_root_source: /Users/husam/.config/facts-tool/config.yaml
facts_template_source: /Users/husam/.config/facts-tool/config.yaml
discovery:
- /Users/.../libtooling-lab/.facts-tool.yaml [absent]
- /Users/husam/.config/facts-tool/config.yaml [found]
```

**What this tells you:** with the working directory anywhere under the
repo and no explicit `--facts`, `facts_template` resolves to a fixed cache
path. A leftover, schema-incompatible database left at that path can break
`import` and other catalog mutations with `incompatible-symbol-universe`
even though your command line never references it, because commands that
consult `facts_template` for invalidation purposes do so independently of
your explicit `--conf`. Run `config show` before a large batch to see what
would be used implicitly, and pass `--facts` explicitly on `import` at this
scale rather than relying on the template.

### 5. Project, user, and explicit config files all merge, per key rather than per file

```console
$ cat .facts-tool.yaml
conf_root: ./.facts-index
conf_template: "{project_name}.db"
extra_args: [-std=c++20, -DPROJECT_LOCAL=1]
$ facts-tool config show
conf_root_source: .../proj/.facts-tool.yaml
extra_args_source: .../proj/.facts-tool.yaml
extra_args: [-std=c++20] [-DPROJECT_LOCAL=1]
discovery:
- .../proj/.facts-tool.yaml [found]
- /Users/husam/.config/facts-tool/config.yaml [found]

$ facts-tool config show --config override.yaml   # override.yaml: extra_args: [-std=c++23]
extra_args_source: .../proj/.facts-tool.yaml, .../override.yaml
extra_args: [-std=c++20] [-DPROJECT_LOCAL=1] [-std=c++23]
discovery:
- .../override.yaml [found]
- .../proj/.facts-tool.yaml [found]
- /Users/husam/.config/facts-tool/config.yaml [found]
```

**What this tells you:** `extra_args` **concatenates** across tiers, in the
order user file, project file, explicit `--config` file, rather than the
highest-precedence tier replacing the lower ones. This is the same
per-key merge behavior documented in
[03-configuration-files](../02-projects-and-configuration/03-configuration-files.md#extra_args-merge-rule),
confirmed live across three tiers at once.

### 6. A CLI `--extra-arg` only "sticks" for the invocation it's given to

Verified with a source that requires C++20 concepts, against a project
file pinning an older standard:

```console
$ cat .facts-tool.yaml
extra_args: [-std=c++11]
$ facts-tool import --conf p2.db --facts f2.db --extra-arg=-std=c++20 cxx20_feature.cpp
Imported 1 compile command(s)
$ facts-tool extract --conf p2.db -o f2.db cxx20_feature.cpp        # NOTE: no --extra-arg here
error: unknown type name 'T'   # concepts feature not usable, extraction fails
$ facts-tool extract --conf p2.db -o f2.db --extra-arg=-std=c++20 cxx20_feature.cpp
facts-tool: 4 symbol(s) recorded from 1 file(s)   # succeeds
```

**What this tells you:** `import --extra-arg` only affects the compile
command stored by that `import`. `extract` (and `analyse dependency`) is a
separate compiler consumer that recomputes its own merged `extra_args`
from YAML independently, unless you repeat the same `--extra-arg` on that
invocation too, even though the project database already stored a
working command from `import`. Here the project's `.facts-tool.yaml`
(`-std=c++11`, too old for concepts) silently won over what `import` had
baked in and broke `extract`. Keep `--extra-arg` consistent across
`import` and every later `extract`/`dependency`/`match` invocation on the
same sources, or keep the flag in YAML instead of on the command line.

## Pitfalls

- **`-p` on `facts-tool-batch` unions with explicit sources; it never
  filters.** Use `--files-from a-list.txt`, or explicit source arguments
  alone without `-p`, to keep a batch scoped to a handful of files.
- **The output directory is exclusively locked per invocation.** A second
  concurrent batch run against the same directory fails fast with "output
  directory is already locked" rather than racing.
- **A stale, incompatible `facts_template`-resolved cache database can
  break `import`** with `incompatible-symbol-universe`, even when you
  never referenced its path on the command line. Always pass `--facts`
  explicitly at scale, and run `config show` first if a failure looks
  unrelated to your actual command.
- **`extra_args` concatenate across YAML tiers by default, and CLI
  `--extra-arg` overrides matching options while preserving unrelated
  defaults for that invocation only.** It does not carry into a later `extract`/`dependency`
  call against the same source.

## Where to go next

- [Batch Processing](../03-extracting-facts/04-batch-processing.md) for the
  full `facts-tool-batch` option reference and output layout.
- [Configuration Files](../02-projects-and-configuration/03-configuration-files.md)
  for the full precedence model behind steps 4 to 6.
- [The config Command](../02-projects-and-configuration/04-config-command.md)
  for `config show`'s complete field reference.
- [Using facts-tool from an AI Agent](08-using-facts-tool-from-an-ai-agent.md)
  for why an agent should run `config show` before any large-codebase
  query too.
