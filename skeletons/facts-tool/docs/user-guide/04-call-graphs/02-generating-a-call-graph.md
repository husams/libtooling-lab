# Generating a Call Graph

This chapter documents every option of `analyse call-graph`, plus the two
companion commands that read a single function's evidence
(`analyse call-graph-entry`) and build the include-dependency graph
(`analyse dependency`).

## `analyse call-graph` - full option reference

`facts-tool analyse call-graph --help`, with the shared `-h`/`-v`/`-c`/
`--config` entries and the wrapped help text of each option elided:

```text
Traverse a contextual function call graph and persist the run in the facts
database (callgraph_run tables); prints one completion line

facts-tool analyse call-graph [OPTIONS]

OPTIONS:
  -f,     --facts FILE        SQLite facts database
          --direction DIRECTION:{callees,callers}
          --to TARGET         Exact qualified name or USR path target
          --path-mode MODE:{shortest,all-simple}
          --component NAME    Include an explicitly named project component; repeatable
          --calls-scope SCOPE:{all,project,library}
          --max-depth N:INT in [1 - 2147483647]
          --max-nodes N:POSITIVE
          --max-edges N:POSITIVE
          --time-limit-ms N:POSITIVE
          --recover-missing   Recover missing project graph evidence
[Option Group: scope]
  Select graph roots
  [Exactly 1 of the following options are required]

OPTIONS:
          --function SELECTOR Qualified function name or USR
          --all               All definition-backed functions with calls
```

### `--function` / `--all`

Exactly one is required. `--function` accepts an exact qualified name or a
USR. `--all` treats every definition-backed function with recorded calls as
its own root - useful for a project-wide traversal rather than a single
entry point.

### `--direction`

`callees` (default) walks what the root calls; `callers` reverses the
traversal to walk what calls into the root. See
[Overview](01-overview.md#direction-callees-vs-callers) for a worked
example of both.

### `--to` and `--path-mode`

`--to TARGET` turns the traversal into a reachability/path query between the
root(s) and one target (exact qualified name or USR). The two runs below come from the sample project the SDK chapters use
(`project.sqlite` plus `facts.sqlite`), which has an `app::run` entry point.
Run ids keep counting up across a facts store's whole history, so yours will
differ:

```console
$ facts-tool analyse call-graph -c project.sqlite -f facts.sqlite --function app::run --to app::persist
facts-tool: call graph run 4 complete
$ facts-tool analyse call-graph -c project.sqlite -f facts.sqlite --function app::run --to app::dispatch_probe
facts-tool: call graph run 5 complete
```

`--path-mode shortest` (default) returns one minimum-hop path using
canonical USR and relation-site tie-breaks. `--path-mode all-simple`
returns every deterministically ordered node-simple path - cycles cannot
produce infinite results, and a source equal to its target is a valid
zero-edge path. `--path-mode` requires `--to`.

A path run persists only the edges on the found path(s) when at least one
path exists. When the target is unreachable, the run still completes and
instead keeps every edge the search actually explored - a non-empty edge
set on such a run does **not** mean a path was found. Determine this
through a public reader's `target_reached`/`path_outcome` field (see
[Entries, Runs, and Status](03-entries-runs-and-status.md)); do not infer
reachability from edge count alone. The two runs above make the point: the
reachable `app::persist` run kept 2 edges and reports
`path_outcome='found'`, while the unreachable `app::dispatch_probe` run kept
*more* edges (3) and reports `path_outcome='unreachable'` with
`target_reached=False`. Both are `status='complete'`.

`--direction callers` cannot be combined with `--to`; `--to` cannot be
combined with `--all`.

### `--component` and `--calls-scope`

`--component NAME` (repeatable) restricts the traversal to the union of the
named components. `--calls-scope {all,project,library}` (default `all`)
further restricts by whether an endpoint belongs to the project or an
external library. Both require a matching project catalog (`-c/--conf`).
These filters cut at excluded endpoints and report the observed boundary
identity - they never walk through an excluded node just to reconnect a
permitted one on the other side. Selected roots are subject to the same
filters, so an out-of-scope root is reported as excluded, and
`--calls-scope library` needs a library-side root to produce anything.

### Budgets: `--max-depth`, `--max-nodes`, `--max-edges`, `--time-limit-ms`

All four are optional and request-only - there is no persisted default cap.
See [Overview](01-overview.md#budgets-and-depth) for their exact semantics
and a worked truncation example. An exact-depth leaf with no qualifying
outgoing edge is reported as complete, not truncated.

### `--recover-missing`

Opt-in flag that allows the command to write new facts by extracting
registered-but-unindexed translation units into an isolated, temporary
facts store during the traversal. Without it, `analyse call-graph` is
strictly read-only. See
[Recovery and Boundaries](04-recovery-and-boundaries.md) for the full
recovery contract.

### `-f`/`--facts` and `-c`/`--conf`

`-f/--facts` is the facts database to traverse (defaults to
`facts_template` when omitted). `-c/--conf` supplies the matching project
catalog; when present, the command validates the project/facts identity
pair and resolves source paths for recovery. Without `--conf`, roots and
recovery are limited to what the facts store alone can resolve.

## `analyse call-graph-entry`

Looks up one function's collected-evidence state without generating
anything new - a read-only lookup, even without `--recover-missing`.

`facts-tool analyse call-graph-entry --help`, with the shared
`-h`/`-v`/`-c`/`--config` entries elided:

```text
Inspect one collected function entry

facts-tool analyse call-graph-entry [OPTIONS]

OPTIONS:
  -f,     --facts FILE REQUIRED
                              SQLite facts database
          --function SELECTOR REQUIRED
                              Qualified function name or USR
          --format FORMAT:{text,json}
                              Output representation: text or json
```

The default `--format text` prints two lines:

```console
$ facts-tool analyse call-graph-entry -c demo.db -f demo-facts.db --function main
symbol_id=4294967298 usr=c:@F@main# entry_available=false graph_node_ref=null is_leaf=null
external_targets=16 pair=validated definition_availability=available extraction_coverage=incomplete freshness=unknown unresolved_targets=0
```

`--format json` emits the same record as one compact line. Pretty-printed,
with the 16 `external_targets` entries reduced to the first one:

```json
{
  "coverage": {"action": "reconcile-coverage-metadata", "catalog_indexed": false, "catalog_mtime": null, "failure": null, "freshness": "unknown", "indexed_at": null, "state": "unknown", "unresolved_targets": 0},
  "definition_availability": "available",
  "entry_available": false,
  "external_targets": [
    {"external_symbol_id": "3152505995266", "name": "std::vector::vector<_Tp, _Alloc>", "site": {"column": 47, "destination_id": "3152505995266", "file_id": 1, "kind": 1, "line": 21, "offset": 385, "position": 0, "source_id": "4294967298"}, "symbol_id": "3152505995266", "usr": "c:@N@std@N@__1@ST>2#T#T@vector@F@vector#"}
  ],
  "extraction_coverage": {"failure": null, "recovery_candidates": [], "state": "incomplete"},
  "graph_node_ref": null,
  "is_leaf": null,
  "pair": {"state": "validated"},
  "schema_version": 1,
  "symbol_id": "4294967298",
  "usr": "c:@F@main#"
}
```

`symbol_id`, `graph_node_ref`, and every symbol ID inside `external_targets`
(including the ones nested under `site`) are **decimal strings**, not JSON
numbers - this matters if you parse the output programmatically. `file_id`,
`line`, `column`, `kind`, and `position` stay numbers.
`entry_available: false` here reflects this exact demo's command order: a
function entry is only published by `extract` (a "committed generation"),
never by `analyse call-graph` or `match`. If you run `call-graph-entry`
before ever running `extract` against that function, or after a mutation
invalidated its entry, expect `entry_available: false` regardless of
whether the function was reached by a graph traversal. See
[Entries, Runs, and Status](03-entries-runs-and-status.md#function-entries)
for the full `entry_available`/`is_leaf` state table.

`root-not-found` and `ambiguous-root` errors exit 2, with candidate USRs
listed for disambiguation. This is the same selector logic `analyse
call-graph` uses, with one wording difference: `call-graph-entry` renames the
"not found" case to `root-not-found`, while `analyse call-graph` reports it
as `missing-root`. Both are prefixed `facts-tool: usage error: `.

## `analyse dependency`

Builds the direct `#include` dependency graph for one or more translation
units, independent of the call graph:

`facts-tool analyse dependency --help`, with the shared `-h`/`-v`/`-c`/
`--config` entries elided:

```text
Build the direct include dependency graph

facts-tool analyse dependency [OPTIONS] sources...

POSITIONALS:
  sources TEXT ... REQUIRED   Translation-unit roots to analyse

OPTIONS:
  -o,     --output FILE       SQLite database for extracted dependency facts; defaults to
                              facts_template when omitted
          --extra-arg ARG     Compiler argument replacing YAML extra_args; shell-tokenized and
                              repeatable
```

```console
$ facts-tool analyse dependency -c demo.db -o demo-dep.db proj/src/main.cpp -v 1
facts-tool: dependency: starting
facts-tool: dependency: validate sources
facts-tool: dependency: validate database paths
facts-tool: dependency: load compilation database
facts-tool: dependency: validate stored commands
facts-tool: dependency: analyse dependency graph
facts-tool: dependency: open project database
facts-tool: dependency: register files
facts-tool: dependency: collect includes
facts-tool: dependency: register included files
facts-tool: dependency: resolve registered files
facts-tool: dependency: build graph
facts-tool: dependency: persist graph
facts-tool: dependency: complete
```

Like `analyse call-graph`, this writes into the facts database
(`include_dependency` table) rather than printing a graph to stdout; read
it back through the public Python SDK where supported; if the installed SDK
does not expose the required evidence, report that capability gap and never
fall back to SQL or a database driver. `sources` is required - unlike
`extract`, there is no "extract everything registered" default for this
command. `facts-tool-batch dependency` (see
[Batch Processing](../03-extracting-facts/04-batch-processing.md)) runs
this same command per source with bounded parallelism, one independent
output database per source.
