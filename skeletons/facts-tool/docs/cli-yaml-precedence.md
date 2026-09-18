# CLI and YAML precedence

Every explicit CLI value overrides the corresponding YAML default for that
invocation. Omitted CLI options leave YAML defaults available per setting;
built-in values apply only when the relevant setting is absent from both.
Compiler extras override matching YAML options while retaining unrelated defaults. It leaves compilation-database arguments and YAML tier order intact.

## Supported option matrix

The YAML schema contains six keys, including the boolean `ast_cache`.
There are no YAML defaults for verbosity, selectors, source lists, matcher
expressions, traversal depth, or compilation-database directories.

| YAML setting | CLI override | Commands | Omitted CLI / built-in behavior |
| --- | --- | --- | --- |
| `conf_root`, `conf_template` | `-c`, `--conf` | `import`, `extract`, `analyse dependency` | Resolve the YAML-generated project DB; absent keys use the data directory and `{relative_path}/{filename}.db`. |
| `conf_root`, `conf_template` | `-c`, `--conf` | `repo`, `component`, `dir`, `file` groups and leaves | The same project DB resolver serves catalog reads and writes. |
| `conf_root`, `conf_template` | `-c`, `--conf` | `config show` | Display resolved values and provenance without creating storage. |
| `conf_root`, `conf_template` | `-c`, `--conf` | `match`, every `analyse` leaf, and `symbol` group and leaves | Select the project configuration DB independently of an explicit or configured facts DB. |
| `facts_template` | `-o`, `--output` | `extract`, `analyse dependency` | Render the YAML facts path; no built-in facts template. |
| `facts_template` | `-f`, `--facts` | `symbol list` / `ls`, `show`, `browser`; group or leaf | Render a project-scoped YAML template; source-dependent templates require an explicit facts path. |
| `facts_template` | `-f`, `--facts` | `match`, `analyse call-graph`, `analyse call-graph-entry` | Render the YAML facts path when omitted; analysis without a source selector needs a project-scoped template. |
| `extra_args` | repeated `--extra-arg` | `import`, `extract`, `analyse dependency` | Use merged YAML tokens; the built-in list is empty. |
| `ast_cache` | YAML only | Every command that parses translation units | Disabled by default; selected file > project > user > built-in, including explicit `false`. |
| `ast_cache_dir` | YAML only | Every command that parses translation units; `config show` | Defaults to `<project_root>/.facts-tool/ast-cache`; relative directories anchor to project root and `~/` expands `HOME`. |

`--config` selects a YAML file and overrides `FACTS_TOOL_CONFIG` as the
selector on commands accepting configuration options. That selected file
still participates above project and user YAML. `FACTS_TOOL_CONF` remains
below explicit `--conf` and above generated project DB naming.

`--conf`/`-c` and `--config` are optional throughout the command tree.
`match`, call-graph analysis, entry inspection, and symbol inspection discover
the project database even when `--facts` is supplied. An explicit facts path
overrides only the facts path, leaving project configuration discovery intact.
This also makes configured project metadata and call-graph recovery available
without repeating `--conf`.

When no project database is selected, configured, or present at the generated
default path, an explicit facts path can still be used for standalone reads
or the legacy combined project/facts `match` workflow. Invalid configuration
and missing explicitly configured databases do not trigger that fallback.
`config show`
reports YAML extras but has no `--extra-arg` option. The override rule does not
introduce new flags or keys.

## Presence, lists, and persistence

Explicit path values must not silently become omission because they equal a
default-like string. Empty `--conf`, `--config`, `--output`, and `--facts`
values are errors; they never request YAML fallback.
For repeatable arguments, whitespace-only fragments yield no overrides and leave YAML defaults intact.
An explicit YAML `ast_cache: false` overrides a lower-tier `true`.
Cache keys have no CLI enable/disable or directory overrides.

Without CLI extras, YAML lists concatenate user, project, then selected file.
With CLI extras, shell-tokenize the fragments once and remove only matching
YAML options before appending CLI tokens in occurrence order. Macro definitions
and undefinitions match by macro name. Clang option aliases and joined/separate
operands share an option identity; all YAML occurrences of an overridden option
are replaced by the CLI occurrences. Unrelated `-D`, `-I`, and `-include`
options remain available. YAML files are never rewritten. Preserve repeated CLI switches and option/operand
adjacency; compiler conflict semantics within the retained command remain
the compiler's responsibility.

The original compilation-database command remains the base. Import persists
that command with explicit import-time CLI extras, excluding runtime YAML
defaults. Subsequent extraction/dependency invocations combine their current
YAML defaults and CLI overrides without overwriting the stored base or accumulating YAML.

## Executable coverage

The registered pytest-bdd suite exercises real CLI invocations and checks
database paths, stored commands, symbols, and discovered includes.
`cli_yaml_precedence.feature` verifies per-option overrides, unchanged YAML files, and YAML
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
