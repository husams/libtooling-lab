# How to use the native facts-tool CLI

The native executable creates and maintains paired project and facts stores.
The Python package is the read-only reasoning layer over those stores.

## Create the paired databases

Import compile commands into the project database, then extract facts from the
stored commands:

```console
facts-tool import --conf project.sqlite --facts facts.sqlite -p build
facts-tool extract --conf project.sqlite --output facts.sqlite
```

Pass source paths to limit either command. Inspect resolved YAML and database
paths without mutation:

```console
facts-tool config show
facts-tool config show --config ./team.yaml
```

## Analyse or add facts

```console
facts-tool analyse dependency --conf project.sqlite --output deps.sqlite src/main.cpp
facts-tool analyse call-graph --facts facts.sqlite --function app::run \
  --max-depth 4
facts-tool match --conf project.sqlite --facts facts.sqlite \
  --matcher 'functionDecl(isDefinition()).bind("symbol")' src/main.cpp
facts-tool match --conf project.sqlite --facts facts.sqlite \
  --relation-kind Calls \
  --matcher 'callExpr(callee(functionDecl().bind("callee"))).bind("call")' \
  src/main.cpp
```

`analyse dependency` writes direct include facts. `analyse call-graph` reads a
facts database unless `--recover-missing` is explicitly requested. It prints
one completion line and persists an append-only run; parse its `run_id` and
pass that exact id to `cb.callgraphs.get(run_id)` in the installed SDK.
`match` persists bound facts; invalid binding sets fail before facts
are committed. Inspect each command's `--help` for the current binding and
relation options.

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

The catalog groups also provide registration, clone switching, versioning, file
option editing, and removal commands. Read the project-management guide before
catalog writes; use `--dry-run` where offered. Read the configuration guide for
`--conf`, `--config`, YAML precedence, and generated database paths.

After any import, extraction, matcher write, catalog change, or checkout switch,
reopen the Python `CodeBase` so its read-only connections see a coherent pair.
