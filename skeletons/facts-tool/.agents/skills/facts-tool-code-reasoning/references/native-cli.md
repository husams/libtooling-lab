# How to use the native facts-tool CLI

The native executable creates and maintains paired project and facts stores.
The Python package is the read-only reasoning layer over those stores, and it
also provides a standalone reader for successful matcher JSON documents.

## Use configured database paths

Let the discovered YAML configuration resolve the project and facts database
paths. Run `facts-tool config show` first; add `--config ./team.yaml` only
when selecting a specific YAML file. Do not require database-path flags for
normal commands. Explicit `--conf`, `--facts`, and `--output` are optional
overrides for a deliberate alternate store or isolated fixture.
Record those explicit overrides with the command: `config show` reports
configuration provenance/defaults, not flags passed to a later invocation.
The native operation validates the actual selected pair.

## Create the paired databases

Import compile commands into the project database, then extract facts from the
stored commands:

```console
facts-tool import -p build
facts-tool extract
```

Pass source paths to limit either command. Inspect resolved YAML and database
paths without mutation:

```console
facts-tool config show
facts-tool config show --config ./team.yaml
```

## Analyse or add facts

```console
facts-tool analyse dependency src/main.cpp
facts-tool analyse call-graph --function app::run \
  --max-depth 4
facts-tool match \
  --matcher 'functionDecl(isDefinition()).bind("symbol")' src/main.cpp
facts-tool match \
  --relation-kind Calls \
  --matcher 'callExpr(callee(functionDecl().bind("callee"))).bind("call")' \
  src/main.cpp
```

Before depending on structured results, preflight both surfaces in the
selected environment: inspect `facts-tool match --help` for `--format json`,
then verify that the installed Python package exports `MatchResults` and
`load_match_results`. An installed package or binary may differ from the
checkout, so verify both capabilities instead of inferring them from docs.

For the exact invocation result, optionally save structured JSON while keeping
the normal text mode as the default:

```console
facts-tool match --format json \
  --matcher 'functionDecl(hasName("main")).bind("symbol")' \
  src/main.cpp > matches.json
```

Read `matches.json` with the public SDK and inspect each actual binding's
identity, translation-unit provenance, optional location, range, and
unavailable-location reason; see [match-results.md](match-results.md).

`analyse dependency` writes direct include facts. `analyse call-graph` reads a
facts database unless `--recover-missing` is explicitly requested. It prints
one completion line and persists an append-only run; parse its `run_id` and
pass that exact id to `cb.callgraphs.get(run_id)` in the installed SDK.
`match` persists bound facts; invalid binding sets fail before facts
are committed. Inspect each command's `--help` for the current binding and
relation options.

## Inspect and manage the project catalog

```console
facts-tool repo list
facts-tool component list
facts-tool component compile-commands app
facts-tool dir list --component app
facts-tool file show src/main.cpp
facts-tool symbol show app::run
facts-tool symbol find --name app::run
facts-tool symbol find --usr 'EXACT_USR'
facts-tool symbol browser
```

`symbol find` reads the matched-symbol discovery index and does not parse AST
source. The catalog groups also provide registration, clone switching,
versioning, file option editing, and removal commands. Read the
project-management guide before catalog writes; use `--dry-run` where offered.
Read the configuration guide for `--conf`, `--config`, YAML precedence, and
generated database paths.

Before querying persisted stores after an import, extraction, matcher write,
catalog change, or checkout switch, reopen the Python `CodeBase` so its
read-only connections see a coherent pair. The standalone matcher JSON reader
does not open `CodeBase`.
