# Project configuration defaults

facts-tool reads optional YAML defaults with yaml-cpp 0.9.0. The first file
found wins: `--config`, `FACTS_TOOL_CONFIG`, the nearest project
`.facts-tool.yaml`, `$XDG_CONFIG_HOME/facts-tool/config.yaml`, then
`$HOME/.config/facts-tool/config.yaml`.

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

`extra_args` entries are complete compiler tokens. Repeatable CLI
`--extra-arg` values are shell-tokenized once and appended after YAML tokens;
they preserve duplicates and option/operand order. The effective command is
base compile options, then YAML tokens, then flattened CLI fragments. Import
stores the original compile options plus explicit CLI fragments, while
extraction and dependency analysis apply current YAML defaults at runtime.

Inspect resolution without creating files:

```text
facts-tool config show
facts-tool config show --config ./team.yaml
```

Configuration errors exit 3; usage errors exit 2 and runtime failures exit 1.
