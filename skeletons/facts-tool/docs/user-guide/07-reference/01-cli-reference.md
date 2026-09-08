# CLI reference

Reference for every `facts-tool` subcommand: synopsis, positionals, options,
exit codes, and one example. All output shown was captured from a real run
against `$FT/build/facts-tool` (never `~/.local/bin/facts-tool`), trimmed for
length but not invented. For narrative walkthroughs, see
[extract](../03-extracting-facts/01-extract.md),
[importing compile commands](../02-projects-and-configuration/02-importing-compile-commands.md),
[matchers](../03-extracting-facts/03-match-dynamic-matchers.md), and
[call graphs](../04-call-graphs/01-overview.md).

`facts-tool` has 10 top-level subcommands: `extract`, `import`, `match`,
`analyse` (3 leaves), `repo` (7 leaves), `component` (6 leaves), `dir` (2
leaves), `file` (6 leaves), `symbol` (5 leaves, one of which - `index` - has
its own leaf `clear`), and `config` (1 leaf `show`).

## Exit-code contract

Every subcommand shares one exit-code contract
(`src/cli/Dispatch.cpp:142`, function `report()`), which prefixes every
error with `facts-tool: ` and then dispatches on the error text's prefix:

| Exit | Meaning | Trigger |
|---|---|---|
| `0` | success | the command's own return value on success (includes a `truncated` call-graph run - see [call graphs](../04-call-graphs/01-overview.md)) |
| `1` | runtime / database / operational error | any failure whose message does not start with `configuration error:`, `usage error:`, or `cancelled` - e.g. a missing database file, a failed traversal after it started |
| `2` | usage error | bad CLI input the parser or a command validates - unknown flag, invalid selector, `--to` combined with `--all` or with `--direction callers`, an ambiguous root/entry name, an empty `--conf`/`--config`/`--output`/`--facts` value |
| `3` | configuration error | invalid or contradictory configuration - a malformed YAML file, `--recover-missing` without a project configuration, `--component`/`--calls-scope` without a project/facts pair, an empty `FACTS_TOOL_CONF`/`FACTS_TOOL_CONFIG`, a relative `XDG_CONFIG_HOME`/`XDG_DATA_HOME` |
| `130` | cancelled (`SIGINT`) | interrupted before or during a long-running operation such as `analyse call-graph` |

A `--conf` (or resolved `conf_template`) path that does not exist is always a
database error (exit `1`), never a configuration error - this holds for
every command, verified for `repo list` and for an explicit nonexistent
`-c` path.

This is a global contract, not specific to any one command - `analyse
call-graph`'s own docs happen to spell out the full outcome matrix, but the
same three prefixes (`configuration error:`, `usage error:`, `cancelled`)
govern every subcommand's exit code.

## Environment variables

| Variable | Effect | Precedence |
|---|---|---|
| `FACTS_TOOL_CONF` | Direct path to the project database, bypassing `conf_root`/`conf_template` entirely - same role as `--conf` | Below `--conf`; wins over YAML and built-ins. An explicit but empty value is a configuration error. |
| `FACTS_TOOL_CONFIG` | Path to a YAML defaults file, same role as `--config` | Below `--config`; wins over the project `.facts-tool.yaml` and the user config file. An explicit but empty value is a configuration error. A missing file at this tier is a configuration error (unlike the project/user tiers, where a missing file is not an error). |
| `XDG_CONFIG_HOME` | Base directory for the user config file, read as `$XDG_CONFIG_HOME/facts-tool/config.yaml` | Falls back to `$HOME/.config/facts-tool/config.yaml` when unset. Must be an absolute path - a relative value is a configuration error. |
| `XDG_DATA_HOME` | Base directory for generated project/facts databases, used to fill the built-in `conf_root` (`$XDG_DATA_HOME/facts-tool`) | Falls back to `$HOME/.local/share/facts-tool` when unset. Must be an absolute path. |
| `HOME` | Fallback base for both of the above when the matching `XDG_*` variable is unset | A missing `HOME` is only an error if an explicit value or the matching `XDG_*` variable does not already supply what's needed. |

`facts-tool config show` prints every effective key together with which tier
supplied it, and the full discovery trail, without creating any file - see
[the config command](../02-projects-and-configuration/04-config-command.md).
Full precedence and YAML schema are in
[configuration files](../02-projects-and-configuration/03-configuration-files.md).

## Shared options

Every leaf command accepts the same three configuration flags. Their help
text is reproduced once here and elided below as **[[config-help]]**:

| Flag | Type | Default | Meaning |
|---|---|---|---|
| `-c`, `--conf` | `FILE` | generated from `conf_template` | Direct project DB path: overrides `FACTS_TOOL_CONF` and generated naming/ownership. Compiler extras use YAML when CLI `--extra-arg` is omitted. |
| `--config` | `FILE` | none | YAML defaults file (yaml-cpp 0.9.0). Every explicitly supplied CLI value overrides its YAML value; omitted CLI values fall back to YAML, then built-ins. Files merge **per key**, highest precedence first: `--config`/`FACTS_TOOL_CONFIG` file, nearest project `.facts-tool.yaml`, then the user file (`$XDG_CONFIG_HOME/facts-tool/config.yaml` or `$HOME/.config/facts-tool/config.yaml`). |
| `-h`, `--help` | flag | - | Print help and exit |

Every leaf command except `config show` also takes:

| Flag | Type | Default | Meaning |
|---|---|---|---|
| `-v`, `--verbose` | `INT` in `[0-3]` | `1` | Verbosity: `0`=quiet, `1`=stages, `2`=details, `3`=trace |

`extract`, `import`, and `analyse dependency` additionally take
(`match` does **not** accept `--extra-arg`):

| Flag | Type | Default | Meaning |
|---|---|---|---|
| `--extra-arg` | `ARG` (repeatable) | merged YAML `extra_args` | Compiler argument; shell-tokenized. Supplying any `--extra-arg` **replaces** the entire merged YAML `extra_args` list, not just conflicting tokens. |

`repo`, `component`, `dir`, and `file` groups additionally take a
**group-level** flag:

| Flag | Type | Default | Meaning |
|---|---|---|---|
| `-f`, `--facts` | `FILE` | none | Existing facts database whose call-graph entries are invalidated before the catalog mutation commits (see [call-graph-entries.md](../../call-graph-entries.md) invalidation rules, summarized in [limitations](04-limitations-and-known-issues.md)). |

`symbol`'s leaves take the same `-f`/`--facts` flag, but scoped to reading
(`--facts` defaults to `facts_template` when omitted; project-scoped
templates only).

## `extract`

Extract facts using a stored project configuration.

```text
facts-tool extract [OPTIONS] [sources...]
```

**Positionals**

| Positional | Meaning |
|---|---|
| `sources` (0 or more) | Source files to extract; defaults to all imported files |

**Options**

| Flag | Type | Default | Meaning |
|---|---|---|---|
| `-o`, `--output` | `FILE` | `facts_template` | SQLite database for extracted facts |
| `-c`, `--conf` | `FILE` | - | [[config-help]] |
| `--config` | `FILE` | - | [[config-help]] |
| `-v`, `--verbose` | `INT [0-3]` | `1` | Verbosity |
| `--extra-arg` | `ARG` (repeatable) | YAML `extra_args` | Compiler argument, replaces YAML extras |

**Exit codes**: standard contract above.

**Example**

Default verbosity, with the interleaved
`facts-tool: coverage.unsupported_semantics ...` lines removed:

```console
$ facts-tool extract -c demo.db -o demo-facts.db -v 1 proj/src/shapes.cpp proj/src/main.cpp
facts-tool: extract: starting
facts-tool: extract: validate database paths
facts-tool: extract: load compilation database
facts-tool: extract: validate stored commands
facts-tool: extract: extract facts
facts-tool: extract: open project database
facts-tool: extract: validate registry completeness
facts-tool: extract: select sources
facts-tool: extract: resolve registered sources
[1/2] Processing file .../proj/src/shapes.cpp.
[2/2] Processing file .../proj/src/main.cpp.
facts-tool: extract: configure Clang tool
facts-tool: extract: open output database
facts-tool: extract: begin output transaction
facts-tool: extract: Clang parse and AST extraction
[1/2] Processing file .../proj/src/shapes.cpp.
[2/2] Processing file .../proj/src/main.cpp.
facts-tool: extract: commit output transaction
facts-tool: 83 symbol(s) recorded from 24 file(s)
facts-tool: extract: complete
```

The `[n/2] Processing file` block appears twice: once while resolving
registered sources and once during the Clang parse. `-v 2` adds
`facts-tool: extract: configuration=..., output=..., requested_sources=2`
and `facts-tool: extract: selected_sources=2` to the same sequence.

An ordinary C++ file that includes the standard library also prints
`facts-tool: coverage.unsupported_semantics kind=implicit-cleanup site=...`
once per implicit-destructor call site the extractor could not attribute a
precise source column to (usually libc++ internals) - 31 of them for this
two-file demo. These lines are **not** verbosity-gated: they appear at
`-v 0` as well. They are routine and do not affect the exit code or the
recorded symbol count. Everything `extract` writes, including the final
`N symbol(s) recorded` summary, goes to stderr, not stdout.

## `import`

Import compile commands into a project configuration.

```text
facts-tool import [OPTIONS] [sources...]
```

**Positionals**

| Positional | Meaning |
|---|---|
| `sources` (0 or more) | Source files to import; filters compilation database commands, or supply raw sources with `--extra-arg` |

**Options**

| Flag | Type | Default | Meaning |
|---|---|---|---|
| `-f`, `--facts` | `FILE` | none | Existing facts database whose entries must be invalidated before this mutation |
| `-c`, `--conf` | `FILE` | - | [[config-help]] |
| `--config` | `FILE` | - | [[config-help]] |
| `-v`, `--verbose` | `INT [0-3]` | `1` | Verbosity |
| `-p`, `--compilation-database` | `DIR` | none | Directory containing `compile_commands.json` |
| `--component` | `NAME=PATH` (repeatable) | none | Project component as `name=path` |
| `--extra-arg` | `ARG` (repeatable) | YAML `extra_args` | Compiler argument for fixed-command or `compile_commands.json` imports |

**Exit codes**: standard contract above.

**Example**

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

`import` discovers and registers every transitively `#include`d file, not
just the sources you name - a 2-file, 2-header demo project registered 820
files (every libc++/SDK header it pulled in). See
[limitations](04-limitations-and-known-issues.md) for the component-creation
pitfall this command has.

## `match`

Run a dynamic AST matcher and persist bound facts.

```text
facts-tool match [OPTIONS] [sources...]
```

**Positionals**

| Positional | Meaning |
|---|---|
| `sources` (0 or more) | Translation units relative to the invocation directory; defaults to imported order |

**Options**

| Flag | Type | Default | Meaning |
|---|---|---|---|
| `-f`, `--facts` | `FILE` | `facts_template` | SQLite facts database |
| `-c`, `--conf` | `FILE` | - | [[config-help]] |
| `--config` | `FILE` | - | [[config-help]] |
| `-v`, `--verbose` | `INT [0-3]` | `1` | Verbosity |
| `--matcher` | `EXPR` (required) | - | Clang dynamic matcher expression; bind `symbol`, `call`+`callee`, or `source`+`target`[+`site`] |
| `--relation-kind` | `KIND` | - | Relation kind for `source`/`target` bindings; required for relation contracts |

**Exit codes**: standard contract above.

**Example**

```console
$ facts-tool match -c demo.db -f demo-facts.db \
    --matcher 'cxxMethodDecl(hasName("area"), ofClass(hasName("Circle"))).bind("symbol")' \
    proj/src/shapes.cpp -v 1
facts-tool: match: starting
symbol kind=method name=shapes::Circle::area
symbol kind=method name=shapes::Circle::area
facts-tool: 2 symbol(s) recorded from 1 file(s)
facts-tool: match: complete
```

This populates `matched_symbol_index` in the **project** database, never the
facts database - see [storage schema](02-storage-schema.md) and
[matcher chapter](../03-extracting-facts/03-match-dynamic-matchers.md).
Binding `source` alone (without `target`) fails contract validation.

## `analyse`

Parent group for explicitly requested analyses.

```text
facts-tool analyse [OPTIONS] SUBCOMMAND
```

| Subcommand | Description |
|---|---|
| `dependency` | Build the direct include dependency graph |
| `call-graph` | Traverse a contextual function call graph and persist the run |
| `call-graph-entry` | Inspect one collected function entry |

### `analyse dependency`

Build the direct include dependency graph.

```text
facts-tool analyse dependency [OPTIONS] sources...
```

**Positionals**

| Positional | Meaning |
|---|---|
| `sources` (1 or more, required) | Translation-unit roots to analyse |

**Options**

| Flag | Type | Default | Meaning |
|---|---|---|---|
| `-o`, `--output` | `FILE` | `facts_template` | SQLite database for extracted dependency facts |
| `-c`, `--conf` | `FILE` | - | [[config-help]] |
| `--config` | `FILE` | - | [[config-help]] |
| `-v`, `--verbose` | `INT [0-3]` | `1` | Verbosity |
| `--extra-arg` | `ARG` (repeatable) | YAML `extra_args` | Compiler argument |

**Exit codes**: standard contract above.

**Example**

```console
$ facts-tool analyse dependency -c demo.db -o demo-dep.db proj/src/main.cpp -v 1
facts-tool: dependency: starting
facts-tool: dependency: validate sources
facts-tool: dependency: validate database paths
facts-tool: dependency: load compilation database
facts-tool: dependency: validate stored commands
facts-tool: dependency: analyse dependency graph
facts-tool: dependency: open project database
facts-tool: dependency: register files
facts-tool: dependency: collect includes
facts-tool: dependency: register included files
facts-tool: dependency: resolve registered files
facts-tool: dependency: build graph
facts-tool: dependency: persist graph
facts-tool: dependency: complete
```

Writes the `include_dependency` table (see
[storage schema](02-storage-schema.md)); no CLI surface prints it back - read
it with the Python SDK or `sqlite3`.

### `analyse call-graph`

Traverse a contextual function call graph and persist the run in the facts
database (`callgraph_run*` tables). Prints exactly one completion line.

```text
facts-tool analyse call-graph [OPTIONS]
```

No positionals; graph roots are selected through the required `scope` option
group below.

**Options**

| Flag | Type | Default | Meaning |
|---|---|---|---|
| `-f`, `--facts` | `FILE` | - | SQLite facts database |
| `-c`, `--conf` | `FILE` | - | [[config-help]] |
| `--config` | `FILE` | - | [[config-help]] |
| `-v`, `--verbose` | `INT [0-3]` | `1` | Verbosity |
| `--direction` | `{callees,callers}` | `callees` | Traversal direction |
| `--to` | `TARGET` | none | Exact qualified name or USR path target |
| `--path-mode` | `{shortest,all-simple}` | `shortest` | Path selection when `--to` is given |
| `--component` | `NAME` (repeatable) | all | Include an explicitly named project component |
| `--calls-scope` | `{all,project,library}` | `all` | Endpoint scope |
| `--max-depth` | `INT [1, 2147483647]` | unbounded | Maximum call-edge hop depth |
| `--max-nodes` | positive `INT` | unbounded | Maximum distinct canonical node count |
| `--max-edges` | positive `INT` | unbounded | Maximum distinct canonical edge count |
| `--time-limit-ms` | positive `INT` | unbounded | Monotonic traversal time limit |
| `--recover-missing` | flag | off | Recover missing project graph evidence (see [recovery](../04-call-graphs/04-recovery-and-boundaries.md)) |

**Option group `scope`** (exactly one required):

| Flag | Type | Meaning |
|---|---|---|
| `--function` | `SELECTOR` | Qualified function name or USR |
| `--all` | flag | All definition-backed functions with calls |

**Exit codes**: standard contract above. A `truncated` run still exits `0`.
See the run-status table in
[entries, runs, and status](../04-call-graphs/03-entries-runs-and-status.md#runs-identity-and-status)
for what each of `complete`/`truncated`/`cancelled`/`recovery-failed`/
`failed` means and which exit code it carries.

**Example**

```console
$ facts-tool analyse call-graph -c demo.db -f demo-facts.db --function main -v 1
facts-tool: call-graph: starting
facts-tool: roots selected
facts-tool: graph traversal
facts-tool: call-graph: complete
facts-tool: call graph run 1 complete
```

`analyse call-graph` never prints the graph itself - read the persisted run
back via SQLite or the Python SDK's `cb.callgraphs` reader (schema 12 only);
see [storage schema](02-storage-schema.md) and
[persisted call-graph runs](../05-python-sdk/06-persisted-callgraph-runs.md).

### `analyse call-graph-entry`

Inspect one collected function entry.

```text
facts-tool analyse call-graph-entry [OPTIONS]
```

**Options**

| Flag | Type | Default | Meaning |
|---|---|---|---|
| `-f`, `--facts` | `FILE` (required) | - | SQLite facts database |
| `-c`, `--conf` | `FILE` | - | [[config-help]] |
| `--config` | `FILE` | - | [[config-help]] |
| `-v`, `--verbose` | `INT [0-3]` | `1` | Verbosity |
| `--function` | `SELECTOR` (required) | - | Qualified function name or USR |
| `--format` | `{text,json}` | `text` | Output representation |

**Exit codes**: standard contract above. `root-not-found`/`ambiguous-root`
exit `2` with candidate USRs listed for disambiguation.

**Example**

```console
$ facts-tool analyse call-graph-entry -c demo.db -f demo-facts.db --function main
symbol_id=4294967298 usr=c:@F@main# entry_available=false graph_node_ref=null is_leaf=null
external_targets=16 pair=validated definition_availability=available extraction_coverage=incomplete freshness=unknown unresolved_targets=0
```

`--format json` emits the same record as one compact line with keys in
alphabetical order. See
[generating a call graph](../04-call-graphs/02-generating-a-call-graph.md#analyse-call-graph-entry)
for the pretty-printed form. `symbol_id`, `graph_node_ref`, and every symbol
ID inside `external_targets` are decimal-string values in JSON, never
numbers; `file_id`, `line`, `column`, `kind`, and `position` stay numbers.
`entry_available: false` is expected unless a prior `extract` published a
fully-collected body for that function - see
[function entries](../04-call-graphs/03-entries-runs-and-status.md).

## `repo` - manage repositories and checkout clones

```text
facts-tool repo [OPTIONS] SUBCOMMAND
```

Group-level options: `-f`/`--facts`, `-c`/`--conf`, `--config`,
`-v`/`--verbose` (all as in [Shared options](#shared-options)).

| Leaf | Positionals | Extra options |
|---|---|---|
| `list` (alias `ls`) | - | - |
| `show` | `name` (required) | - |
| `add` | `name`, `path` (both required) | `--label TEXT`, `--remote TEXT` |
| `add-clone` | `name`, `path` (both required) | `--label TEXT` |
| `switch` | `name`, `target` (both required) | - |
| `rm-clone` (alias `remove-clone`) | `name`, `target` (both required) | - |
| `rm` | `name` (required) | `--delete-components`, `--dry-run` |

**Exit codes**: standard contract above.

**Examples**

```console
$ facts-tool repo add demo "$PROJ" -c demo.db
Repository registered

$ facts-tool repo list -c demo.db
ID  NAME  KIND  COMPONENTS  CLONES  ACTIVE CLONE
...

$ facts-tool repo show demo -c demo.db
# prints a header, a "Clones (* active):" block, and a nested component table
```

`add-clone`, `switch`, `rm-clone`, and `rm` were not exercised in this
research pass; their synopsis and options above come directly from `--help`.
`repo rm-clone`/`remove-clone` documents that it cannot remove the
active/only clone.

## `component` - manage project components

```text
facts-tool component [OPTIONS] SUBCOMMAND
```

Group-level options: same shared set as `repo`.

| Leaf | Positionals | Extra options |
|---|---|---|
| `list` (alias `ls`) | - | - |
| `show` | `name` (required) | - |
| `add` | - | `--path DIR` (required), `--name TEXT`, `--repo TEXT`, `--kind {repo,external}`, `--version TEXT`, `--no-git` |
| `set-version` | `name` (required), `version` (optional) | omit `version` to clear it |
| `compile-commands` | `name` (required) | exports stored commands as JSON |
| `rm` | - | selector group (exactly one of `--id INT`, `--path TEXT`, `--name TEXT`), `--dry-run` |

**Exit codes**: standard contract above.

**Example**

```console
$ facts-tool component add --path "$PROJ" --name demo --repo demo --kind repo -c project.db
Component registered
```

A **built-in component** (`kind=external`, root `/`, id `1`) already exists
in a project database the moment the first `repo`/`component`/`import`
command creates it, before you register anything of your own. It is named
`facts-tool` at that point; an `import` that registers out-of-project
headers renames it in place to `external` and files those headers under it.
`component list`/`component show` can fail on a
component `import` created implicitly - see
[limitations](04-limitations-and-known-issues.md). `set-version`,
`compile-commands`, and `rm` were not exercised this session.

## `dir` - manage indexed directories

```text
facts-tool dir [OPTIONS] SUBCOMMAND
```

| Leaf | Positionals | Extra options |
|---|---|---|
| `list` (alias `ls`) | - | `--component NAME` |
| `rm` | - | selector group (exactly one of `--id INT`, `--path TEXT`), `--component TEXT`, `--dry-run` |

**Exit codes**: standard contract above.

**Example**

```console
$ facts-tool dir list -c demo.db
ID  COMPONENT  FILES  PATH
...
```

After an `import` that pulled in the standard library, `dir list` shows one
row per registered directory - the project directories plus dozens of
`external`-kind directories under the compiler's SDK/resource paths. This is
expected, not a defect. `dir rm` was not exercised this session.

## `file` - manage registered files

```text
facts-tool file [OPTIONS] SUBCOMMAND
```

| Leaf | Positionals | Extra options |
|---|---|---|
| `list` (alias `ls`) | - | - |
| `show` | `path` (required) | - |
| `add` | `path` (required) | `--driver TEXT` (required), `--working-directory TEXT`, `--arg TOKEN` (repeatable) |
| `rm` (alias `remove`) | `path` (required) | - |
| `set-option` | - | `--match REGEX` (required), `--arg TOKEN` (required, repeatable) |
| `clear-option` | - | `--match REGEX` (required), `--arg TOKEN` (required, repeatable) |

**Exit codes**: standard contract above.

**Example**

```console
$ facts-tool file list -c demo.db
ID  COMPONENT  DIRECTORY  FILE  OVERRIDDEN  INDEXED  PATH
...
```

`file add` hand-registers one file with a fixed driver (for a
non-`compile_commands.json`-driven source); `import` supersedes it for the
normal workflow. `set-option`/`clear-option` match is a case-sensitive
ECMAScript `regex_search` over the forward-slash-normalized path relative to
the component root, with exact contiguous-token matching, and roll back the
whole operation on an invalid or unmatched expression. `show`, `add`, `rm`,
`set-option`, and `clear-option` were not exercised in this research pass.

## `symbol` - inspect extracted symbols

```text
facts-tool symbol [OPTIONS] SUBCOMMAND
```

Group-level options: `-f`/`--facts` (defaults to `facts_template`,
project-scoped templates only), `-c`/`--conf` (optional - enables full
source-path and repository/component join columns), `--config`,
`-v`/`--verbose`.

| Leaf | Positionals | Extra options |
|---|---|---|
| `list` (alias `ls`) | - | - |
| `show` | `qualified-name` (required) | - |
| `browser` | - | interactive; no non-interactive mode |
| `find` | - | selector group (exactly one of `--usr TEXT`, `--name TEXT`), `--kind INT`, `--format {text,json}` |
| `index` | - | parent group for `clear` |
| `index clear` | - | `--file-id INT` (required, `>= 1`) |

**Exit codes**: standard contract above.

**Examples**

```console
$ facts-tool symbol list -c demo.db -f demo-facts.db
kind                qualified name
namespace           (anonymous)
function            (anonymous namespace)::totalArea(const std::vector& items) -> double
function            main() -> int
...

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

$ facts-tool symbol find -c demo.db --name area
USR                             QUALIFIED NAME        FILE ID  KIND  PATH                          COMPONENT  REPOSITORY
c:@N@shapes@S@Circle@F@area#1   shapes::Circle::area  2        17    .../proj/src/shapes.cpp       proj       proj
c:@N@shapes@S@Circle@F@area#1   shapes::Circle::area  820      17    .../proj/include/shapes.hpp   proj       proj
INDEX SCOPE: matched-only
SOURCE COMPLETE: unknown

$ facts-tool symbol index clear -c demo.db --file-id 2
```

`identity 820:7` is `<file_id>:<index>` (Symbol identity, not a line/column
pair). `symbol find` reads `matched_symbol_index`, which is populated only by
`match`, never `extract` - an empty result never proves a symbol is absent
from source. `symbol browser` is interactive and was not exercised in this
research pass.

## `config show`

Show resolved YAML defaults and ordered discovery, without creating storage.

```text
facts-tool config show [OPTIONS]
```

**Options**

| Flag | Type | Default | Meaning |
|---|---|---|---|
| `-c`, `--conf` | `FILE` | - | [[config-help]] |
| `--config` | `FILE` | - | [[config-help]] |

**Exit codes**: standard contract above.

**Example**

```console
$ facts-tool config show
parser: YAML / yaml-cpp 0.9.0
project_root: ".../cli-demo"
conf: "/Users/husam/.cache/facts/cli-demo/project.db"
conf_root: "~/.cache/facts"
conf_template: {project_name}/project.db
facts_template: ~/.cache/facts/{project_name}/facts.db
source: generated
conf_root_source: /Users/husam/.config/facts-tool/config.yaml
conf_template_source: /Users/husam/.config/facts-tool/config.yaml
facts_template_source: /Users/husam/.config/facts-tool/config.yaml
extra_args_source: built-in
extra_args:
discovery:
- .../cli-demo/.facts-tool.yaml [absent]
- /Users/husam/.config/facts-tool/config.yaml [found]
```

Full precedence rules, YAML schema, and templating are in
[configuration files](../02-projects-and-configuration/03-configuration-files.md).

## Related: `facts-tool-batch`

`facts-tool-batch` is a separate Python wrapper script (not part of the
`facts-tool` binary) that fans a native `facts-tool extract`/`analyse
dependency` invocation out per source file with bounded parallelism. See
[batch processing](../03-extracting-facts/04-batch-processing.md).
