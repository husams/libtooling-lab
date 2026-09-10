# Troubleshooting

Symptom -> cause -> fix, for every error message captured while researching
this guide. Native CLI errors always follow the
[global exit-code contract](01-cli-reference.md#exit-code-contract). SDK
errors are `FactsToolError(code, message)` with `str(exc) == f"{code}:
{message}"`.

## Native CLI errors

| Symptom | Cause | Fix |
|---|---|---|
| `facts-tool: project configuration database not found: <path>` (exit 1) | `--conf`/resolved `conf_template` points at a file that does not exist | Run `facts-tool config show` to see the resolved path, then `repo add`/`import` to create it, or point `--conf` at the right file. A nonexistent `--conf` path is always a database error (exit 1), never a configuration error. |
| `facts-tool: usage error: The following argument was not expected: <flag>` (exit 2) | An unrecognized flag for that command | Check [the CLI reference](01-cli-reference.md) for that command's exact option set |
| `facts-tool: usage error: --to is incompatible with --all` / `--to is incompatible with --direction callers` (exit 2) | A flag combination `analyse call-graph` rejects after parsing | Pick a single root with `--function` for a `--to` query, and leave `--direction` at its `callees` default |
| `facts-tool: usage error: -c,--conf must not be empty` / `--config must not be empty` / `-o/--output must not be empty` / `--facts must not be empty` (exit 2) | The flag was supplied with an empty string | Drop the flag to use the generated template, or give it a real path. An empty *environment* variable is exit 3 instead, not exit 2. |
| `facts-tool: incompatible-symbol-universe: unsupported facts schema version` (exit 1) | The facts database exceeds the supported schema version, checked during call-graph-entry invalidation | Re-import/extract into a fresh facts store. One occurrence in research was not reproducible after ~6 further attempts with the same flags - if it recurs, capture `PRAGMA user_version` on both databases before retrying. |
| `facts-tool: ambiguous stored compile commands for requested source '<path>'` (exit 1) | Two catalog paths registered compile commands for the same source - typically a manual `component add` followed by a plain `import -p DIR` for the same checkout | Do not combine manual `component add` with `import -p DIR` for the same directory. Use `repo add` (for clone tracking) followed by plain `import -p DIR` with no `--component` flag and no prior `component add`. See [limitations](04-limitations-and-known-issues.md). |
| `facts-tool: component has no active clone: <name>` from `component list`/`component show` | A component that `import -p DIR` (or `import --component NAME=PATH`) created implicitly has no `repo_id` link to an actual repository/clone | Expected after a plain `import -p DIR`; nothing else in the workflow (`dir list`, `file list`, `extract`, `symbol`, `match`, `analyse call-graph`) depends on this succeeding. See [limitations](04-limitations-and-known-issues.md). |
| `facts-tool: ambiguous component` from `component show` | `import --component NAME=PATH` created a **second** component with the same name as a pre-existing manually-added one | Don't pre-create a component with `component add` before an `import --component` that reuses its name; `import` never reuses an existing component, it always creates a new one. |
| `facts-tool: indexing incomplete: cannot persist relation=template_instance source='...' target='...' usr='<unavailable>': Invalid argument` (exit 1), whole extraction rolled back to 0 symbols | A class-template specialization is **named** without being **required complete** (`TSK_Undeclared`, e.g. `Holder<Widget> *p;`, a reference-only parameter, an alias, or a field of pointer type) elsewhere in the same translation unit | Known, apparently still-open limitation (B-019, no recorded fix at the time of writing). See [limitations](04-limitations-and-known-issues.md). Isolate the offending declaration into its own translation unit if this blocks extraction of the rest of a file. |
| `facts-tool: indexing incomplete: cannot extract reference to '...(anonymous union at ...)': invalid USR` / `...(anonymous struct at ...)` | Historically (B-021, fixed): a `MemberExpr` reaching a member of an anonymous union/struct through an unnamed intermediate field | Fixed on main - such references are now skipped, not fatal. If you see this on current main, treat it as a regression and capture a fixture. |
| `facts-tool: indexing incomplete: cannot persist relation=method_of/field_of source='...' target='...': target symbol is not persisted` | Historically (B-021, fixed): an out-of-line member/field/enumerator whose owning record lives in a filtered system header | Fixed on main - such owners now get a persisted `is_external=1` stub keyed by USR. |
| SIGSEGV / exit 139 during extraction, inside `EvaluateAsRValue()` | Historically (B-020, fixed): a dependent `alignof(T)` (or similar) inside an uninstantiated template reached the constant evaluator before substitution | Fixed on main - dependent expressions are now rejected before evaluation and persisted with `evaluated_kind='none'`. If you see a crash here on current main, treat it as a regression. |
| `facts-tool: usage error: missing-root: selector '<name>' was not found` (exit 2), or `root-not-found: ...` from `analyse call-graph-entry` | `--function` matched no stored qualified name and no stored USR. The two commands use the same selector logic but different wording for this case | The selector is matched exactly, with no substring or unqualified fallback. Use `facts-tool symbol list` or `symbol show` to get the exact qualified name, or pass the USR. |
| `facts-tool: usage error: ambiguous-root: selector '<name>' matches [...]; select a USR` (exit 2) | `--function` matched more than one symbol, typically overloads sharing a qualified name | The error lists every candidate USR; pick one and pass it as `--function`. |
| `facts-tool: recovery failed for N translation unit(s); see callgraph_run_recovery run <id>` (exit 1, run status `recovery-failed`) | `--recover-missing` tried to compile a registered TU and it failed | Use the public Python SDK: read `cb.callgraphs.get(run_id).recovery`; each item's `diagnostic` holds the captured compiler output for the failing TU. The run still keeps every edge reached before the failure. |
| `facts-tool: cancelled before traversal` (exit 130, no run written) | `SIGINT` arrived before the pre-traversal checkpoint | Nothing persisted; re-run when ready. |
| `facts-tool: cancelled; run <id> keeps the last usable generation` (exit 130, run status `cancelled`) | `SIGINT` arrived during traversal or recovery | The run row and its edges up to the last usable generation are kept; read them like any other run. |
| `facts-tool: cannot persist call graph run: <SQLite text>` (exit 1, no run written) | The final commit failed - e.g. the facts store is read-only | Check file permissions and disk space; no partial run or child rows are ever written on this path. |

## Configuration errors

| Symptom | Cause | Fix |
|---|---|---|
| Generated project/facts paths land somewhere unexpected (e.g. under `~/.cache/facts/...`) | A user-level `~/.config/facts-tool/config.yaml` (or `$XDG_CONFIG_HOME/facts-tool/config.yaml`) on the machine redirects generated names via `conf_root`/`conf_template`/`facts_template` | Run `facts-tool config show` first whenever a generated path looks unexpected; it reports which tier supplied each key. `-c`/`-o`/`-f` always override the templates explicitly. |
| `facts-tool: configuration error: ...` (exit 3) for an unrelated tier's file | An existing-but-invalid YAML file at *any* precedence tier is a configuration error, even when a higher tier would have won the merge | Every tier is still checked and reported; fix or remove the invalid file at the tier `config show`'s discovery list marks `[invalid]`. A *missing* file at any tier (except an explicit `--config`/`FACTS_TOOL_CONFIG`) is not an error. |
| `FACTS_TOOL_CONF must not be empty` / `FACTS_TOOL_CONFIG must not be empty` (exit 3) | The environment variable is set but empty | Unset it or give it a real path |
| `XDG_CONFIG_HOME must be absolute` / relative `XDG_DATA_HOME` rejected | `XDG_CONFIG_HOME`/`XDG_DATA_HOME` is set to a relative path | Set it to an absolute path, or unset it to fall back to `$HOME` |
| A `--extra-arg` you passed silently dropped an unrelated YAML flag | An older binary may still replace the whole YAML list | Update to per-option override behavior: matching values are overridden at runtime, unrelated defaults survive, and YAML files remain unchanged |

## SDK errors

| Code | Meaning | Fix |
|---|---|---|
| `E_DATABASE` | Path is missing, not a regular file, corrupt, or cannot be opened read-only | The SDK never creates or repairs a database; check the path. Quote shell paths with spaces (Python path objects need no special handling). |
| `E_DATABASE_ROLE` | Required tables don't match the supplied role, or both arguments resolve to the same physical file | Confirm `facts_db` points at extraction output and `project_db` at the project/configuration registry; they must be different files |
| `E_SCHEMA` | Facts `user_version` is unsupported (not 10, 11, or 12), or a schema-12 store is missing a required `callgraph_run*` table/column | Re-extract with a current `facts-tool` build. Real text: `facts schema user_version 5 is unsupported; need 10, 11, or 12`. Don't trust `python/docs/databases.md`'s "10 and 11" claim - see [storage schema](02-storage-schema.md). |
| `E_DATABASE_PAIR` | FileIds referenced by facts are absent from the project registry - the two databases weren't produced by the same indexing run | Open the correct facts/project pair together. Real text: `project database lacks FileIds: [2]`. |
| `E_SOURCE` | Symbol ref not found, or (for `EntityQuery.get`) resolved via the client-side "first persisted match wins" rule rather than an ambiguity error | Prefer an exact USR or fully-qualified name. See the ambiguity caveat below. |
| `E_FIELD`, `E_VIEW`, `E_KIND` | A catalog field/view/kind name typo in `select`/`where`/`order_by`/predicates | Check [views](../05-python-sdk/04-views-and-catalog.md) and [relations](../05-python-sdk/05-relations-and-graph-queries.md) for valid names |
| `E_RELATION` | Invalid relation name in a graph request | Same as above; check the relation catalog |
| `E_DEPTH` | A traversal depth window is outside `1..32` | Real text: `invalid depth window 1..999; maximum is 32`. Lower the requested depth. The `32` ceiling is hard-coded in `queryplan/validate_predicates.py`; raising `Budgets.max_depth` past it does **not** lift this check. |
| `E_LIMIT` | Non-positive `limit`, negative/bool cursor/offset/`after`, or `result_cap < 1` | Pass a positive `int` (not `bool`) for every paging argument |
| `E_BUDGET` | Plan depth exceeds the budget's pre-check in `Executor.run` | Real text: `plan depth exceeds the executor budget`. Raise `Budgets.max_depth` when opening the codebase, up to the hard `32` ceiling `E_DEPTH` enforces. |
| `E_SETOP` | Mismatched node view across `union_`/`intersect`/`except_` | Ensure both operands enumerate the same view before combining |
| `E_STAGE` | Invalid stage order or a missing operand (e.g. `view("site")` immediately after `sites()`) | Reorder stages; `sites()` output cannot be re-viewed |
| `E_UNKNOWN` | `unknown="error"` policy hit on a predicate whose evidence a truncated traversal couldn't prove | Either raise the traversal budget, or pass `unknown="include"`/`"exclude"` instead of `"error"` |
| `E_CAPABILITY` | The request needs semantics `facts-tool` doesn't store - devirtualized-mode traversal, a call-graph-run reader against schema < 12, or cidx-only semantics | Real text: `persisted call graph runs require facts schema 12`. Re-extract into a schema-12 store, or query direct relations instead. |
| `E_IDENTITY` | An out-of-range packed `SymbolId` half (`symbol identity halves must be unsigned 32-bit`), or a FileId a path lookup required but the project DB does not have (`FileId 5 is absent from project database`) | Verify the ref you constructed, and confirm you opened the matching facts/project pair. Both cases are on `main`; [in-flight F-013](05-in-flight-f-013.md) adds an ambiguous-ref case on the unmerged S-031 branch. |

### Ambiguous symbol refs are not an error (contradicts some docs)

`docs/quickstart.md` and `docs/cidx-migration.md` each claim different
"ambiguity" behavior for a `symbol(ref)` source, and neither matches the
actual implementation. Verified directly: `start(symbol("app::save"))`
against a project with two `app::save` overloads returns **both** matching
rows as separate nodes; `cb.get(ref)`/`cb.query(ref)` (`GraphQuery.get`)
silently take the **first persisted match**, with no ambiguity check
anywhere in `source_exec.py` or `view_symbols.py`. Don't rely on either
doc's claim - use an exact USR when you need one specific overload.

### `is_template()` is dead code on real data

`queryplan.helpers.is_template()` checks `kind in ("class_template",
"function_template")`, but the persisted `clang::index::SymbolKind` catalog
never contains those strings - a real templated struct reports `kind ==
"struct"`. Verified: `nodes(is_template())` against a real demo project
(with `Box`/`Holder` templates) returned zero rows. Use `is_instance()`
(checks the `instantiates` relation) or a direct `eq("kind", "struct")`
instead.

## Schema and pairing mismatches

| Symptom | Cause | Fix |
|---|---|---|
| A facts DB and project DB open successfully but `provenance.pairing == "unverifiable"` | This is the **normal, expected** result for a successful open - numeric `FileId` overlap alone is never proof two stores came from the same indexing run | No action needed unless the native writer separately reports `incompatible-symbol-universe`. |
| A facts DB migrated from schema 10/11 up to 12 doesn't behave like a store that was always at 12 | Migration adds new tables but never invents historical provenance/entry data for facts extracted before the migration | Re-extract every source into the migrated store if you need fresh call-graph entries or provenance evidence, rather than trusting the migrated rows as complete. |

## Match / matcher errors

| Symptom | Cause | Fix |
|---|---|---|
| `match` accepted the command but bound nothing useful, or a contract-validation error | Binding `source` alone (without `target`) fails contract validation - the shipped contract requires exactly one of `symbol`, `call`+`callee`, or `source`+`target`[+`site`, with `--relation-kind` required for the last form] | See [matchers](../03-extracting-facts/03-match-dynamic-matchers.md) for worked examples of each binding form |
| `symbol find` returns nothing for a symbol you know exists | `matched_symbol_index` is populated **only** by a prior successful `match`, never by `extract` | Run a targeted `match --matcher 'functionDecl(hasName("X")).bind("symbol")'` first; an empty `find` result never proves absence |

## Missing SDK headers / resource dir

`facts-tool` never needs `-isysroot`/`-resource-dir` in your
`compile_commands.json`: `src/platform/PlatformFlags.cpp`/
`DriverIncludes.cpp` inject the resource dir (from the linked libClang) and
the SDK path (via `xcrun --show-sdk-path` on macOS) automatically when the
stored command lacks them. Real evidence: a demo project's
`compile_commands.json` carried zero platform flags and `extract`/`analyse
dependency` still succeeded. If you see `'string'`/`'stdarg.h' file not
found` from a *different* LibTooling-based binary in this lab, that tool was
built outside the skeleton pattern that gives `facts-tool` this behavior for
free - it needs its own equivalent platform-flags helper, not something this
guide's `facts-tool` chapters cover.

## Stale `~/.local/bin/facts-tool` vs. `build/facts-tool`

If a command's behavior doesn't match this guide, confirm you are running
`$FT/build/facts-tool` (built at the checkout's current commit), not an
older copy installed at `~/.local/bin/facts-tool`. Both can exist on the
same machine; only the freshly built binary is guaranteed to match this
guide's examples.

## Verbosity and logging

`-v` gates only the `facts-tool: <command>: <stage>` progress lines. It does
**not** gate the Clang tool's `[n/m] Processing file ...` lines, the final
`facts-tool: N symbol(s) recorded from M file(s)` summary, or the
`coverage.unsupported_semantics` notices - all of those print at `-v 0` too.
Everything listed here goes to **stderr**; `extract` writes nothing to
stdout at any level.

- `-v 0`: no stage lines. You still get the `[n/m] Processing file` blocks,
  the recorded-symbols summary, and any `coverage.unsupported_semantics`
  lines.
- `-v 1` (default): stage lines (`facts-tool: <command>: <stage>`).
- `-v 2`: adds per-item detail, e.g. `facts-tool: extract:
  configuration='demo.db', output='demo-facts.db', requested_sources=2` and
  `facts-tool: extract: selected_sources=2`.
- `-v 3`: trace level - every extraction/skip decision, e.g. `facts-tool:
  trace: node extraction kind='Field' name='...' result=filtered
  reason='invalid USR'`.

`facts-tool: coverage.unsupported_semantics kind=implicit-cleanup site=...`
appears once per implicit-destructor call site the extractor could not
attribute a precise source column to. It is routine noise for any TU that
includes the standard library (31 lines for a two-file demo) and does not
affect the exit code or the recorded symbol count.

For `analyse call-graph` specifically: without `-v`, stderr never carries
more than one line even on failure; with `-v`, that same line is present
alongside the verbose stage lines. stdout only ever carries the one-line
completion message or `--help` text - never a graph.

## zsh quoting

Matcher expressions and `--extra-arg` values routinely contain characters
zsh treats specially (`(`, `)`, `!`, unquoted `*`/`?` globs). Always
single-quote a `--matcher` expression and any `--extra-arg` value that
contains parentheses or spaces:

```console
$ facts-tool match --matcher 'cxxMethodDecl(hasName("area"), ofClass(hasName("Circle"))).bind("symbol")' src/shapes.cpp
```

An unquoted `(` in zsh is a syntax error (`zsh: parse error near ...`), not a
`facts-tool` usage error - if you see a shell parse error rather than
`facts-tool: usage error: ...`, check quoting before assuming the tool
rejected the expression.
