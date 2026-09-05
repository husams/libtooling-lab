# CLI and YAML precedence

Every explicit CLI value overrides the corresponding YAML default for that
invocation. Omitted CLI options leave YAML defaults available per setting;
built-in values apply only when the relevant setting is absent from both.
This contract replaces the earlier practice of appending CLI extras to YAML
extras. It leaves compilation-database arguments and YAML tier order intact.

## Supported option matrix

The YAML schema contains exactly four keys. It has no boolean or numeric
settings and no YAML defaults for verbosity, selectors, source lists, matcher
expressions, traversal depth, or compilation-database directories.

| YAML setting | CLI override | Commands | Omitted CLI / built-in behavior |
| --- | --- | --- | --- |
| `conf_root`, `conf_template` | `-c`, `--conf` | `import`, `extract`, `analyse dependency` | Resolve the YAML-generated project DB; absent keys use the data directory and `{relative_path}/{filename}.db`. |
| `conf_root`, `conf_template` | `-c`, `--conf` | `repo`, `component`, `dir`, `file` groups and leaves | The same project DB resolver serves catalog reads and writes. |
| `conf_root`, `conf_template` | `-c`, `--conf` | `config show` | Display resolved values and provenance without creating storage. |
| `conf_root`, `conf_template` | `-c`, `--conf` | `symbol` group and leaves when configuration is consumed | Select the project configuration DB independently of the facts DB. |
| `facts_template` | `-o`, `--output` | `extract`, `analyse dependency` | Render the YAML facts path; no built-in facts template. |
| `facts_template` | `-f`, `--facts` | `symbol list` / `ls`, `show`, `browser`; group or leaf | Render a project-scoped YAML template; source-dependent templates require an explicit facts path. |
| `extra_args` | repeated `--extra-arg` | `import`, `extract`, `analyse dependency` | Use merged YAML tokens; the built-in list is empty. |

`--config` selects a YAML file and overrides `FACTS_TOOL_CONFIG` as the
selector on commands accepting configuration options. That selected file
still participates above project and user YAML. `FACTS_TOOL_CONF` remains
below explicit `--conf` and above generated project DB naming.

`match` and `analyse call-graph` require their own explicit `--facts` and do
not consume YAML defaults. `config show` reports YAML extras but has no
`--extra-arg` option. The override rule does not introduce new flags or keys.

## Presence, lists, and persistence

Explicit path values must not silently become omission because they equal a
default-like string. Empty `--conf`, `--config`, `--output`, and `--facts`
values are errors; they never request YAML fallback.
For repeatable arguments, presence is determined before tokenization: even a
valid fragment yielding no tokens must not reactivate YAML defaults.
False and zero override cases apply only where a CLI option has a supported
YAML counterpart; none of the current YAML fields has a boolean/numeric type.

Without CLI extras, YAML lists concatenate user, project, then selected file.
With CLI extras, use only those fragments, shell-tokenized once in occurrence
order. Replace the whole YAML contribution, including unrelated `-D`, `-I`,
and `-include` options. Preserve repeated CLI switches and option/operand
adjacency; compiler conflict semantics within the retained command remain
the compiler's responsibility.

The original compilation-database command remains the base. Import persists
that command with explicit import-time CLI extras, excluding runtime YAML
defaults. Subsequent extraction/dependency invocations select their current
YAML or CLI extras without overwriting the stored base or accumulating YAML.

## Executable coverage

The registered pytest-bdd suite exercises real CLI invocations and checks
database paths, stored commands, symbols, and discovered includes.
`cli_yaml_precedence.feature` verifies whole-list replacement and YAML
fallback across compiler consumers and YAML sources, plus symbol
facts-template fallback under a direct project DB override.
`cli_yaml_paths.feature` covers explicit output/facts/conf aliases, including
catalog and symbol group/leaf positions. Existing
`configuration_precedence.feature` covers YAML tier ordering and fallback;
`configuration_paths.feature` covers generated paths and facts templates;
`configuration_policy.feature` and `configuration_schema.feature` cover
validation and command-family policy. `configuration_consumers.feature` and
`extra_args_retention.feature` cover compiler consumers, repeated runs,
both compilation-database representations, and base-command preservation.

Run the complete gates from the facts-tool directory:

```sh
ctest --test-dir build --output-on-failure
bash scripts/run-e2e.sh build
```

Backlog B-033 records the final commit, environment, exact commands, counts,
log locations, and criterion-specific evidence for independent review.
