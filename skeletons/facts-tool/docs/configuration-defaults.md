# Project configuration defaults

facts-tool reads optional YAML defaults with yaml-cpp 0.9.0. The first file
found wins: `--config`, `FACTS_TOOL_CONFIG`, the nearest project
`.facts-tool.yaml`, then `$XDG_CONFIG_HOME/facts-tool/config.yaml` when XDG is
nonempty, otherwise `$HOME/.config/facts-tool/config.yaml`. Relative XDG is an
error. Explicit selectors are cwd-relative and missing/unreadable files fail;
lower-priority files are skipped, never merged. Empty selectors/overrides fail.

```yaml
conf_root: ~/.local/share/facts-tool
conf_template: "{relative_path}/{filename}.db"
extra_args:
  - -std=c++23
  - -Iinclude with spaces
```

`conf_root` and `conf_template` generate a database name from the canonical
project root. The only placeholders are `{relative_path}` and `{filename}`;
generated paths must remain below `conf_root`. A generated database records its
owning project, so a collision is rejected. Use `--conf PATH` or
`FACTS_TOOL_CONF` for an existing database and to bypass generated ownership.
`--conf` beats `FACTS_TOOL_CONF`; conf-only commands skip YAML under either.
Compiler consumers still validate YAML/extra_args, ignoring unused path settings.

Identity starts at canonical invocation cwd, stopping at the first ancestor
with `.git` (file/directory) or `.facts-tool.yaml`; without markers it is cwd.
Source names never determine identity. Built-in storage is
`$XDG_DATA_HOME/facts-tool`, or `$HOME/.local/share/facts-tool`; relative data
XDG fails only when consumed. Missing HOME is allowed when explicit/XDG values
suffice. Absent keys use built-ins; explicit null/empty values do not.

Relative YAML roots anchor to the YAML parent, and only leading `~/` expands.
The parent relative to `/` supplies `{relative_path}`; the complete project
basename supplies `{filename}`. `/` uses `_root`, and `/acme` gives `acme.db`.
Known placeholders may repeat or be omitted; no extension is appended.
Substitution is single-pass and preserves spaces, Unicode, dots, and braces in
project names. Repeated separators/dot components normalize; absolute paths,
any `..`, trailing separators, root-only results, invalid braces, backslashes,
NUL/newline, and canonical symlink escapes are rejected before creation.
Parents are rechecked immediately before opening. Concurrent creators serialize;
unowned existing databases and different project owners are rejected.

`extra_args` entries are complete compiler tokens. Repeatable CLI
`--extra-arg` values are shell-tokenized once and appended after YAML tokens;
they preserve duplicates and option/operand order. The effective command is
base compile options, then YAML tokens, then flattened CLI fragments. Import
stores the original compile options plus explicit CLI fragments, while
extraction and dependency analysis apply current YAML defaults at runtime.
For example YAML `['-DNAME=two words']` supplies one token; CLI
`--extra-arg="-DVALUE=1 '-DNAME=two words'"` supplies two. Unterminated quoting
fails; neither form executes a shell or expands environment variables/globs.
Later switches take precedence only where the selected compiler defines that
behavior. Changed YAML defaults require no reimport and never accumulate.

Only one YAML mapping/document is permitted. Empty documents and empty argument
lists are valid. Unknown/duplicate keys, nulls, wrong types, tags, anchors,
aliases, multiple documents, and NUL/newline strings are configuration errors.

Inspect resolution without creating files:

```text
facts-tool config show
facts-tool config show --config ./team.yaml
```

Output identifies the parser, project root, effective keys, per-key sources,
direct/resolved DB, and ordered absent/selected/skipped/inaccessible candidates.
Errors retain partial provenance; unavailable HOME paths remain symbolic.
Read-only consumers never create storage; missing project DBs are runtime errors.
Import and catalog writes may initialize storage before normal entity validation.
Help and symbol commands with only `--facts` do not discover defaults.

Configuration errors exit 3; usage errors exit 2 and runtime failures exit 1.
