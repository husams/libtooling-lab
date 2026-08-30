# Project catalog commands and E2E tests

The real `facts-tool` executable manages repositories, components, indexed
directories, files, and extracted symbol inspection.

## Commands

Append `--conf /path/to/project.sqlite` to each command.

| Group | Subcommands |
| --- | --- |
| `repo` | `list` / `ls`, `show NAME`, `add-clone NAME PATH [--label LABEL]`, `switch NAME LABEL_OR_PATH`, `rm-clone` / `remove-clone NAME LABEL_OR_PATH`, `rm NAME [--delete-components] [--dry-run]` |
| `component` | `list` / `ls`, `show NAME`, `add --path PATH [--name NAME] [--repo REPO] [--kind repo\|external] [--version VERSION] [--no-git]`, `set-version NAME [VERSION]`, `compile-commands NAME`, `rm SELECTOR [--dry-run]` |
| `dir` | `list` / `ls` `[--component COMPONENT]`, `rm SELECTOR [--component COMPONENT] [--dry-run]` |
| `file` | `list` / `ls`, `show PATH`, `add PATH --driver DRIVER [--working-directory DIR] [--arg TOKEN]...`, `rm` / `remove PATH`, `set-option --match REGEX --arg TOKEN...`, `clear-option --match REGEX --arg TOKEN...` |
| `symbol` | `list` / `ls --facts FACTS.sqlite`, `show QUALIFIED_NAME --facts FACTS.sqlite` |

`repo rm-clone NAME LABEL_OR_PATH` (alias `remove-clone`) removes only a
non-active clone registration and never deletes a checkout. The active clone,
including the only clone, must be switched away before it can be removed.

`file add` canonicalizes the source, compiler driver, and optional working
directory. With no working directory it stores the effective root of the
deepest registered indexed directory containing the file. If the file is below
that directory, its actual parent is registered so the stored path round-trips
without dropping intermediate segments. Arguments are stored as exact tokens
and marked as manually overridden. Removal affects only the
catalog row. Empty file lists print `No files registered`.

File option matching is case-sensitive ECMAScript `regex_search` over the
normalized forward-slash file path relative to its effective component root;
patterns are unanchored unless they use `^` or `$`. `set-option` removes every
exact contiguous occurrence of the supplied token sequence and appends it once;
`clear-option` removes every occurrence. Nonmatching files are unchanged and an
invalid or unmatched expression rolls back the operation. A later import
preserves manually overridden files it omits, while a command containing the
same file replaces its fields and resets the override.

Symbol commands open only the database supplied by `--facts` and never require
or use `--conf`. Listing includes packed, file, and per-file symbol identifiers;
show uses an exact qualified-name match and reports all stored symbol details.
Empty lists print `No symbols`. Both operations are read-only.

Component removal requires exactly one of `--id`, `--name`, or `--path`.
Directory removal requires exactly one of `--id` or `--path`. Directory paths
are relative to the component; `--component` resolves ambiguity. Removal
affects the exact indexed directory and its files, not descendant directories.

Registration can create a fresh configuration without importing or extracting.
For repository components, the default name comes from the root directory and
the default repository name is the component name. Git discovery promotes the
root to the enclosing checkout unless `--no-git` is set. External components
keep their requested root and are ungrouped unless `--repo` is supplied.
Components added to an existing repository must be inside its active clone.
A version is empty or one relative path segment; omit it from `set-version` to
clear it. Effective source paths include the version segment.

Adding a clone does not switch an existing active checkout. Switching accepts a
registered label or path and verifies that the checkout contains the registered
files before changing the active clone. Component, directory, and file IDs remain
stable. `compile-commands` emits JSON using persisted compiler arguments and
resolved working directories, without requiring the original JSON database.

Repository removal detaches components while preserving their resolved paths.
`--delete-components` explicitly cascades through their directory and file rows.
All removal commands affect **catalog rows only**: checkout files and a separate
extracted-facts database are untouched. Extracted facts may therefore retain
references to removed catalog entries; reconcile/re-extract facts separately if
that is required.

Queries and previews open the configuration read-only. Writes use transactions,
report success only after commit, and roll back on validation or SQLite errors.
Existing configurations must have a supported schema; management reads do not
silently migrate them.

## Modular implementation

- `src/cli/catalog/`: one parser module per noun group, with shared selectors.
- `src/commands/catalog/`: command orchestration, tabular output, and JSON export.
- `src/storage/catalog/`: catalog reads, registration, clone changes, removal,
  and transaction helpers.
- `src/cli/Dispatch.cpp`: dispatch separated from the existing parser.

Command behavior remains separated from CLI parsing and catalog persistence.

## Real executable BDD coverage

Eight `tests/e2e/features/catalog_*.feature` files contain **67 scenarios after
expanding examples**. Seven step modules reuse two small support modules. There
are no mocks, expected-failure markers, or skip-on-missing-command checks.

Most scenarios start with real import of four temporary C++ sources, then real
extraction into a separate facts database. SQL establishes repository ownership,
versions, ambiguous legacy rows, or failure-injection triggers as **Given** state;
it never supplies a management command's expected result. Fresh registration
scenarios use a real `git init` and an initially absent configuration.

Assertions open new read-only SQLite connections to verify committed rows,
stable IDs, exact cascades, sibling isolation, foreign keys, and integrity.
Management commands must preserve all checkout source bytes and the separate
facts database. Clone-switch scenarios extract from the other checkout using
only persisted commands and verify the symbol's original file ID.

Coverage includes registration and Git discovery, listing/showing, clones by
label/path, versions, JSON export, all removal selectors, read-only previews,
duplicates, missing/ambiguous objects, invalid arguments, incomplete clones, and
rollback after both cascading deletion and a late repository deletion failure.
The built-in `external` component is explicitly covered as a duplicate-name
rejection; successful external registration uses the distinct name `vendor`.

## Run every executable E2E test

Configure with `BUILD_TESTING=ON`, then build and run:

```bash
cmake --build build -j 4
bash scripts/run-e2e.sh build .codex/e2e-management-implementation/macos-final
```

The runner executes both `facts-tool-cli-contract` and `facts-tool-e2e`, ignores
inherited pytest selection flags, propagates failures, and retains CTest logs
plus CTest/BDD JUnit XML. CMake supplies the real compiler and matching Clang PCH
driver. Existing platform-dependent skips are reported, never counted as passes.

Linux-dev is Rocky Linux 9.8 (RHEL-compatible), not Red Hat Enterprise Linux.
Qualification uses an isolated source snapshot there and leaves its saved
checkout untouched. See
[`implementation evidence`](../.codex/e2e-management-implementation/summary.md)
for exact results and snapshot provenance. The older
`.codex/e2e-management-evidence/` directory records the initial red acceptance
baseline; those 23 command failures are not the implementation's final results.
