# Project configuration defaults

facts-tool reads optional YAML defaults with yaml-cpp 0.9.0 and merges them
per key. Precedence, highest first: `--conf`/`--facts`/`-o`/`--extra-arg` on
the CLI, an explicit `--config`/`FACTS_TOOL_CONFIG` file, the nearest project
`.facts-tool.yaml`, the user file at `$XDG_CONFIG_HOME/facts-tool/config.yaml`
(or `$HOME/.config/facts-tool/config.yaml`), then built-in defaults. A missing
file at any tier is not an error; an existing but invalid file at any tier is
a configuration error, even if a higher tier would have won. `extra_args` is
the one exception: instead of one tier winning, it concatenates in a fixed
order (user, then project, then the `--config` file, then CLI `--extra-arg`)
regardless of which tier supplied the other keys. Explicit selectors are
cwd-relative; a missing explicit `--config`/`FACTS_TOOL_CONFIG` file fails.
Relative `XDG_CONFIG_HOME` is an error. Empty selectors/overrides fail.

```yaml
conf_root: ~/.local/share/facts-tool
conf_template: "{relative_path}/{filename}.db"
facts_template: "{project_root}/.index/{relative_path}/{filename}.db"
extra_args:
  - -std=c++23
  - -Iinclude with spaces
```

`conf_root`/`conf_template` generate the project configuration database name;
`facts_template` supplies the default `-o`/`--facts` path for `extract`,
`analyse dependency`, and `symbol` when the command omits it. Both templates
share one placeholder set: `{project_root}` (canonical project base),
`{project_name}` (its basename), `{relative_path}`/`{filename}`, `{user}`
(from `$USER`/`$LOGNAME`, else the passwd entry), and `${ENV_NAME}` for any
environment variable. Substitution is single-pass; braces or `${}` in a
substituted value are never re-interpreted. Unknown placeholders, unmatched
braces, an unset `${ENV_NAME}`, NUL/newline/backslash, and any `..` component
are configuration errors before any file or directory is created.

In `conf_template`, `{relative_path}`/`{filename}` keep their original
meaning: the project directory's parent relative to `/`, and the complete
project basename (`/` uses `_root`). A raw literal absolute template (one
that starts with `/` before any substitution) is always rejected as an
escape attempt. A raw leading `~/` expands against `HOME`, like `conf_root`'s
own `~/` support, and the result is trusted as-is with no containment check.
Otherwise the template is substituted: a relative result joins onto
`conf_root` (itself anchored to the canonical project root when relative,
never to the directory holding the YAML file that declared it) and must stay
strictly inside it, exactly as before; but when a *placeholder* makes the
result absolute — typically a leading `{project_root}` — that value is
trusted as the database's actual location (bypassing `conf_root` entirely),
with only a narrower check that no literal path segment coming after that
leading placeholder escapes it through a symlink. For example
`conf_template: "{project_root}/.index/project.db"` yields
`<root>/.index/project.db` directly. A generated database records its owning
project, so a collision is rejected. Use `--conf PATH` or `FACTS_TOOL_CONF`
for an existing database and to bypass generated naming and ownership;
`--conf` beats `FACTS_TOOL_CONF`, and conf-only (catalog/symbol) commands
skip YAML entirely under either. Compiler consumers (import, extract,
dependency analysis) still load and merge YAML `extra_args` despite a direct
override, ignoring the now-unused `conf_root`/`conf_template` values
(including their validation); `facts_template` is never bypassed by a direct
override, since it governs the separate facts database, not the overridden
project configuration database.

In `facts_template`, `{relative_path}`/`{filename}` instead describe the
analysed source file relative to the project root, with the extension
stripped from `{filename}`. Given
`facts_template: "{project_root}/.index/{relative_path}/{filename}.db"` and a
run over `src/a/b.cpp`, the default output is `<root>/.index/src/a/b.db`. When
the template needs a source placeholder but the run does not resolve to
exactly one source (symbol commands never do; extract/dependency do when zero
or more than one source apply), the command fails with a usage error (exit 2)
asking for `-o`/`--facts`. An explicit `-o`/`--facts` always wins over
`facts_template`, which follows the same absolute/`~/`/relative rules as
`conf_template` above, except a relative result anchors to the project root
directly (there is no separate `facts_root` setting) and an absolute result
is still checked against a symlink escaping through the placeholder that
made it absolute. Parent directories are created only by write consumers
(extract, dependency analysis), only once every other applicable check
(sources, database paths, stored commands) has already passed; `symbol` is
read-only and never creates them.

Identity starts at canonical invocation cwd, stopping at the first ancestor
with `.git` (file/directory) or `.facts-tool.yaml`; without markers it is cwd.
Source names never determine identity. Built-in storage is
`$XDG_DATA_HOME/facts-tool`, or `$HOME/.local/share/facts-tool`; relative data
XDG fails only when consumed. Missing HOME is allowed when explicit/XDG values
suffice. Absent keys use built-ins; explicit null/empty values do not.

Known placeholders may repeat or be omitted; no extension is appended.
Substitution preserves spaces, Unicode, dots, and braces in project names.
Repeated separators/dot components normalize; a raw literal absolute
`conf_template`, any `..`, trailing separators, root-only results, invalid
braces, backslashes, NUL/newline (including inside a substituted
`{user}`/`{project_root}`/`{project_name}`/`${ENV_NAME}` value), and
canonical symlink escapes are rejected before creation. Parents are
rechecked immediately before opening. Concurrent creators serialize; unowned
existing databases and different project owners are rejected.

`extra_args` entries are complete compiler tokens. Repeatable CLI
`--extra-arg` values are shell-tokenized once and appended last; they
preserve duplicates and option/operand order. The effective command is base
compile options, then the merged YAML tokens, then flattened CLI fragments.
Import stores the original compile options plus explicit CLI fragments,
while extraction and dependency analysis apply current merged defaults at
runtime. For example YAML `['-DNAME=two words']` supplies one token; CLI
`--extra-arg="-DVALUE=1 '-DNAME=two words'"` supplies two. Unterminated
quoting fails; neither form executes a shell or expands environment
variables/globs. Later switches take precedence only where the selected
compiler defines that behavior. Changed YAML defaults require no reimport
and never accumulate.

Only one YAML mapping/document is permitted per file. Empty documents and
empty argument lists are valid. Unknown/duplicate keys, nulls, wrong types,
tags, anchors, aliases, multiple documents, and NUL/newline strings are
configuration errors.

Inspect the merged resolution without creating files:

```text
facts-tool config show
facts-tool config show --config ./team.yaml
```

Output identifies the parser, project root, every effective key with the
tier that supplied it (or `built-in`), the direct/resolved DB, and the
ordered discovery list marking every file `[found]`/`[absent]`/`[invalid]`/
`[inaccessible]`. Every tier (an explicit `--config` file, project, user) is
still checked even after an earlier one is found invalid, so an error
retains the full discovery trail rather than stopping at the first problem;
template syntax (unknown placeholders, unmatched braces, an unset
`${ENV_NAME}`) is likewise validated for each tier as it is read, so an
invalid lower tier is still a configuration error even when a higher tier's
value wins the merge. Unavailable HOME paths remain symbolic. Read-only
consumers never create storage; missing project DBs are runtime errors.
Import and catalog writes may initialize storage before normal entity
validation. Help, and symbol/catalog commands given an explicit
`--facts`/`--conf` with no other configuration flags, do not discover
defaults.

Configuration errors exit 3; usage errors exit 2 and runtime failures exit 1.
