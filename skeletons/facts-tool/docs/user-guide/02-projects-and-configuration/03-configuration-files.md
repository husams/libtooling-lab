# Configuration files

`facts-tool` resolves the paths to its two databases, and its compiler
arguments, through a layered configuration model rather than requiring
every flag on every invocation. This chapter documents that model in full:
the precedence order, every configurable key, template placeholders,
environment variables, and the XDG paths involved. Every claim here is
demonstrated with a real `facts-tool config show` run in
[04-config-command](04-config-command.md) - read that chapter alongside
this one if you want to see the resolution happen live.

## Precedence, highest to lowest

Configuration merges **per key**, not per file - a lower-precedence source
can still supply a key that a higher-precedence source didn't set. In
order, highest precedence first:

1. **Explicit CLI values** for this invocation: `--conf`/`-c`,
   `--output`/`-o`, `--facts`/`-f`, and repeated `--extra-arg`.
2. **`FACTS_TOOL_CONF`** - an environment variable holding a direct project
   database path. This bypasses template resolution entirely, the same way
   an explicit `--conf` would.
3. **YAML configuration**, itself three tiers merged per key (highest
   first):
   1. An explicit `--config FILE` flag, or the `FACTS_TOOL_CONFIG`
      environment variable if no `--config` flag was given.
   2. The nearest project `.facts-tool.yaml`, found by walking up from the
      current working directory.
   3. The user file: `$XDG_CONFIG_HOME/facts-tool/config.yaml`, or
      `$HOME/.config/facts-tool/config.yaml` if `XDG_CONFIG_HOME` isn't
      set.
4. **Built-in defaults**: `conf_root` = `$XDG_DATA_HOME/facts-tool` or
   `$HOME/.local/share/facts-tool`; `conf_template` =
   `{relative_path}/{filename}.db`; `facts_template` = none (must come from
   YAML or an explicit `-o`/`-f`); `extra_args` = `[]`.

A missing file at any YAML tier is not an error - it's simply skipped. An
**existing but invalid** file at *any* tier, however, **is** a
configuration error (exit code 3), even if a higher tier would have won
anyway; every tier is still checked and reported for diagnostics.

## The four YAML keys

The YAML schema is exactly four keys. There are no booleans or numeric
settings in YAML, and nothing else is YAML-configurable - verbosity,
source selectors, matcher expressions, and traversal depth are all
CLI-only.

| Key | Meaning |
|---|---|
| `conf_root` | Base directory generated project database paths are rooted under |
| `conf_template` | Template for the generated project database filename |
| `facts_template` | Template for the generated facts database filename |
| `extra_args` | A list of compiler arguments merged into every extraction/import/match/dependency run |

Example user-level file (`~/.config/facts-tool/config.yaml`):

```yaml
# facts-tool user-level defaults.
# Every generated file for a project lives under ~/.cache/facts/<project>/:
#   project.db   project configuration (import, catalog commands)
#   facts.db     extracted facts (extract, analyse dependency, symbol, call-graph)
# <project> is the basename of the project root (nearest .git or .facts-tool.yaml
# above the working directory). Override per run with --conf / -o / --facts.
conf_root: ~/.cache/facts
conf_template: "{project_name}/project.db"
facts_template: "~/.cache/facts/{project_name}/facts.db"
```

A project-level file overriding just one key (`facts_template`), placed at
`.facts-tool.yaml` at the root of a project checkout:

```yaml
facts_template: "{project_root}/.index/{relative_path}/{filename}.db"
```

## Template placeholders

`conf_template` and `facts_template` support these placeholders:

| Placeholder | Meaning |
|---|---|
| `{project_root}` | The resolved project identity root (see below) |
| `{project_name}` | The basename of `{project_root}` |
| `{relative_path}` | The source file's path relative to `{project_root}` |
| `{filename}` | The source file's basename |
| `{user}` | The current user's name |
| `${ENV_NAME}` | Any environment variable, substituted by name |

Substitution is single-pass: a value substituted into a template is never
re-scanned for further placeholders. Path-escape attempts (`..`
components), embedded NUL or newline bytes, and canonical-symlink escapes
out of the intended root are always rejected before any file is created.

## `extra_args` merge rule

Without any CLI `--extra-arg`, the YAML `extra_args` lists from all three
YAML tiers **concatenate**, in order: user file, then project file, then an
explicit `--config` file. CLI `--extra-arg` values override **matching compiler options** in that list
for this invocation; unrelated YAML arguments remain. For example, CLI
`-std=c++23` replaces YAML `-std=c++17` while retaining YAML `-DKEEP=1`.
Macro overrides match by name, and joined/separate option spellings match.
Repeated CLI options retain their order. YAML files themselves never change.

## Project identity

Project identity (used to fill `{project_root}`/`{project_name}`) starts at
the canonical invocation working directory and walks up to the first
ancestor containing `.git` (file or directory) or `.facts-tool.yaml`; if
neither is found, identity is the working directory itself. This identity
walk is independent of `--conf`'s literal value - `facts_template`
resolution keys off this walk even when `--conf` points at a completely
different path than the project root. Source filenames never determine
project identity, only the invocation's working directory does.

## Exit codes related to configuration

| Exit code | Meaning |
|---|---|
| 2 | Usage error: an unrecognized flag, or an empty `--conf`/`--config`/`--output`/`--facts` value (`facts-tool: usage error: -c,--conf must not be empty`) |
| 3 | Configuration error: an existing-but-invalid file at any tier, or certain flag combinations documented per-command |
| 1 | Runtime/database error - notably, a `--conf` path (explicit or resolved) that does not exist is always a database error, not a configuration error |

Verified for the invalid-file rule: with a syntactically broken
`.facts-tool.yaml` in the working directory, `facts-tool config show
--config good.yaml` still exits 3 and names the broken file, even though the
explicit `--config` file supplied every key that would have been used:

```console
$ facts-tool config show --config good.yaml
...
discovery:
- .../good.yaml [found]
- .../.facts-tool.yaml [invalid]
- /Users/husam/.config/facts-tool/config.yaml [found]
facts-tool: configuration error: configuration in .../.facts-tool.yaml: yaml-cpp: error at line 2, column 1: end of sequence flow not found; ...
```

## Discoverability: `facts-tool config show`

Every rule in this chapter is something you can verify without touching
storage, using `facts-tool config show`. See
[04-config-command](04-config-command.md) for the dedicated walkthrough,
including a live demonstration of per-key precedence across all three YAML
tiers at once.

## What's next

[04-config-command](04-config-command.md) is the companion chapter that
walks through `facts-tool config show` itself: its options, its output
fields, and a live three-tier precedence demonstration.
