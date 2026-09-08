# The `config` command

`facts-tool config show` is the primary discoverability and debugging tool
for the configuration model described in
[03-configuration-files](03-configuration-files.md). It prints the fully
resolved values for this invocation - every database path, every template,
every `extra_args` entry - along with which tier supplied each one, and it
does this **without creating any storage**: no database file, no directory,
no side effect of any kind. Run it any time a generated path looks
unexpected, before you assume something is broken.

```console
$ facts-tool config --help
Inspect merged YAML defaults with yaml-cpp 0.9.0: an optional
--config/FACTS_TOOL_CONFIG file, the project file, and the user file (XDG/HOME)
merge per key; --conf overrides generated naming and ownership

facts-tool config [OPTIONS] SUBCOMMAND

OPTIONS:
  -h,     --help              Print this help message and exit

SUBCOMMANDS:
  show                        Show resolved YAML defaults (yaml-cpp 0.9.0) and ordered
                              discovery
```

`config show` also accepts a `--config FILE` flag, exactly like every other
command, to preview what a specific YAML file would resolve to without
switching your shell's environment.

## Reading the output

A verified run with **no flags**, from a directory called `cli-demo`, on a
machine that already has a user-level config at
`~/.config/facts-tool/config.yaml`:

```console
$ facts-tool config show
parser: YAML / yaml-cpp 0.9.0
project_root: ".../scratchpad/cli-demo"
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
- .../scratchpad/cli-demo/.facts-tool.yaml [absent]
- /Users/husam/.config/facts-tool/config.yaml [found]
```

Each field:

- **`project_root`** - the resolved project identity (see
  [03-configuration-files](03-configuration-files.md#project-identity)).
- **`conf`** - the actual resolved project database path this invocation
  would use.
- **`conf_root` / `conf_template` / `facts_template` / `extra_args`** -
  the resolved value of each of the four YAML keys.
- **`source`** - where the `conf` path itself came from. `generated` means
  built from `conf_root` + `conf_template`; an explicit `--conf` shows
  `--conf` (as below); setting the `FACTS_TOOL_CONF` environment variable
  shows `FACTS_TOOL_CONF`.
- **`*_source`** - for each of the four keys, exactly which file (or
  `built-in`, or `--conf`) supplied the value that won.
- **`discovery`** - every YAML file location that was checked, in
  precedence order, tagged `[found]`, `[absent]`, or `[invalid]`. An
  `[invalid]` entry at any tier makes the whole invocation a configuration
  error (exit 3), even when a higher tier supplied every key.

## Explicit `--conf` overrides naming and ownership, but not `facts_template`

```console
$ facts-tool config show --conf demo.db
conf: ".../cli-demo/demo.db"
conf_root: ""
conf_template: {relative_path}/{filename}.db
facts_template: ~/.cache/facts/{project_name}/facts.db
source: --conf
conf_root_source: built-in
conf_template_source: built-in
facts_template_source: /Users/husam/.config/facts-tool/config.yaml
```

An explicit `--conf` value takes over `conf`, and resets `conf_root`/
`conf_template`'s provenance to `built-in` since they're no longer used to
generate anything - but it does **not** affect `facts_template`, which is
independently resolved and still comes from the user config file. This
matches the per-key merge model: `--conf` only ever overrides the one key
it corresponds to.

## Live demonstration: three-tier YAML precedence

To see per-key (not per-file) precedence in action, this sequence writes a
project-level file and then an explicit `--config` file, and shows how
`config show`'s resolution changes at each step.

A project file at `proj/.facts-tool.yaml`:

```yaml
facts_template: "{project_root}/.index/{relative_path}/{filename}.db"
```

An explicit team file at `team.yaml`:

```yaml
extra_args:
  - -std=c++20
  - -DTEAM_BUILD=1
```

Run from inside `proj` (so the project file is discovered):

```console
$ facts-tool config show
facts_template: {project_root}/.index/{relative_path}/{filename}.db
facts_template_source: .../cli-demo/proj/.facts-tool.yaml
extra_args_source: built-in
extra_args:
discovery:
- .../cli-demo/proj/.facts-tool.yaml [found]
- /Users/husam/.config/facts-tool/config.yaml [found]

$ facts-tool config show --config ../team.yaml
facts_template: {project_root}/.index/{relative_path}/{filename}.db   # unchanged: project file still wins for this key
facts_template_source: .../cli-demo/proj/.facts-tool.yaml
extra_args_source: .../cli-demo/team.yaml
extra_args: [-std=c++20] [-DTEAM_BUILD=1]
discovery:
- .../cli-demo/team.yaml [found]
- .../cli-demo/proj/.facts-tool.yaml [found]
- /Users/husam/.config/facts-tool/config.yaml [found]
```

The explicit `--config` file supplied `extra_args`, which won because it's
the highest-precedence YAML tier for that key - but `facts_template` still
came from the project file, because the explicit `--config` file didn't set
that key at all. This is the concrete proof that YAML tiers merge per key,
not per file: supplying one file at the top of the precedence order does
not silently discard every key from lower tiers.

## What's next

With database paths and compiler arguments resolved and understood, move
on to [03-extracting-facts/01-extract](../03-extracting-facts/01-extract.md)
to extract facts from a project, or to
[07-reference/01-cli-reference](../07-reference/01-cli-reference.md) for
the complete option table for every command.
