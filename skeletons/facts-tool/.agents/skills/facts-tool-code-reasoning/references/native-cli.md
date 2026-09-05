# How to use the native facts-tool CLI

The native executable creates and maintains SQLite data. The Python package is
the read-only reasoning layer over the resulting facts and project databases.

## Create the paired databases

Import compile commands into the project database, then extract facts from the
stored commands:

```console
facts-tool import --conf project.sqlite --compilation-database build
facts-tool extract --conf project.sqlite --output facts.sqlite
```

Pass source paths to limit either command. Repeating `--extra-arg` replaces the
complete YAML `extra_args` list while preserving the base compile command.
Inspect resolved YAML and database paths without mutation:

```console
facts-tool config show
facts-tool config show --config ./team.yaml
```

## Analyse or add facts

```console
facts-tool analyse dependency --conf project.sqlite --output deps.sqlite src/main.cpp
facts-tool analyse call-graph --facts facts.sqlite --function app::run --max-depth 4
facts-tool match --facts project-and-facts.sqlite --matcher 'functionDecl().bind("source")'
```

`analyse dependency` writes direct include facts. `analyse call-graph` reads a
facts database. `match` runs a Clang dynamic matcher and persists its bound
facts; inspect `facts-tool match --help` for binding and relation options.

## Inspect and manage the project catalog

```console
facts-tool repo list --conf project.sqlite
facts-tool component list --conf project.sqlite
facts-tool component compile-commands app --conf project.sqlite
facts-tool dir list --component app --conf project.sqlite
facts-tool file show src/main.cpp --conf project.sqlite
facts-tool symbol show app::run --facts facts.sqlite --conf project.sqlite
facts-tool symbol browser --facts facts.sqlite --conf project.sqlite
```

The catalog groups also provide registration, clone switching, versioning,
file option editing, and removal commands. Read the
[project-management guide](../../../../docs/e2e-project-management.md) before
catalog writes; use `--dry-run` where offered and consult
`facts-tool <group> --help` for exact selectors. Read the
[configuration guide](../../../../docs/configuration-defaults.md) for
`--conf`, `--config`, YAML precedence, and generated database paths; read the
[call-graph guide](../../../../docs/call-graph.md) for call-graph output.

After any import, extraction, matcher write, catalog change, or checkout switch,
reopen the Python `CodeBase` so its read-only connections see a coherent pair.
