# Persistent AST cache

Enable persistence in any existing YAML configuration tier:

```yaml
ast_cache: true
ast_cache_dir: .facts-tool/ast-cache
```

Caching is disabled by default. A relative directory is anchored to the
project root; absolute paths and `~/` paths are also supported. Each setting
uses the existing explicit-file, project-file, user-file, then built-in
precedence. `facts-tool config show` reports the values and their provenance.
Configuration inspection does not create the directory.

## Command behavior

| Command | Cache behavior |
| --- | --- |
| `import` | Parses each cache miss once, saving the AST and collecting dependency metadata during that parse. Reuses both at the same Git commit. |
| `extract` | Traverses the AST prepared by import, including on the first extraction. Parses and saves a replacement only when the cache is missing or stale. |
| `match` | Runs the matcher against the shared saved AST. |
| `analyse variable-flow` | Builds its analysis from saved ASTs. |
| `analyse call-graph --recover-missing` | Reuses saved ASTs for recovery scans. |
| `analyse dependency` | Reads the saved dependencies without loading an AST; preprocesses and refreshes missing or stale records. |
| Queries and catalog operations | Do not load or create ASTs. |

Enable caching before import to pay the parsing and dependency discovery cost
during import. The first `extract`, `match`, or analysis then consumes that
prepared cache without preprocessing the source or rescanning its dependencies.
Import processes one translation unit at a time and does not extract semantic
facts. An unchanged reimport checks the saved metadata and artifact integrity
without deserializing the AST or rewriting it.

With caching disabled, import retains its preprocessing-only behavior. If an
enabled import cannot build an AST because of C++ semantic errors, it falls
back to preprocessing for file registration; no invalid AST is published.
Dependency discovery remains preprocessing-only on a miss. Missing, corrupt,
or stale ASTs are repaired when import or an AST consumer next needs them.
Existing extraction freshness checks still apply: use `extract --force` to
request extraction even when the facts database is already current.

Verbose output (`-v 1`) distinguishes `dependency-cache` events from
`ast-cache: miss`, `ast-cache: stored`, and `ast-cache: hit`. Dependency analysis
reads only SQLite metadata on a hit. Warm import also reads the AST bytes to
verify integrity, without deserializing them. Disabled caching performs no
cache reads or writes and does not populate the metadata tables.

## Project database metadata

Metadata is stored as typed SQLite rows in the project configuration database
selected by `--conf`, `FACTS_TOOL_CONF`, or the existing configuration rules.
The facts database keeps its existing role. No JSON metadata file is written
or parsed.

Project schema version 2 adds five tables: `ast_cache_snapshot`,
`ast_cache_input`, `ast_cache_include`, `ast_cache_revision`, and
`ast_cache_artifact`. They record the compile fingerprint, input paths,
include relationships, repository HEAD commits, and the binary AST's path and
integrity digest. Import upgrades existing project databases while preserving
registered file IDs and compile commands.

Each AST is bound to a dependency generation. Refreshing dependency records
for a different generation invalidates the old AST record in the same
transaction. Re-importing unchanged dependencies preserves it. The binary
AST remains `<ast_cache_dir>/<compile-fingerprint>.ast`; legacy JSON sidecars
are ignored and legacy entries are rebuilt when needed.

When a commit changes, import replaces the AST and dependency generation
together so the next consumer can immediately reuse the refreshed artifact.
Existing valid dependency metadata can also repair a missing or corrupt AST
without a separate dependency preprocessing pass.

## Boundaries

The cache directory stores Clang's serialized AST. It does not cache a command's
query result. AST extraction, match selection, and analysis still run on a hit.
Extraction consumes one TU at a time to avoid retaining a project's entire
AST collection in memory.

The cache key includes the source, compilation directory, compiler version,
effective command arguments and relevant environment.
When import expands a response file into stored command arguments, rerun
`import` after changing that file so subsequent commands use the updated arguments.
For an existing entry, repository HEAD commits are the refresh gate. The source
must be tracked in a Git repository with a commit. Dependency records also
retain the commits of other repositories containing included files, including
headers that have not yet been committed in those repositories. Repository
commits for include-search directories are recorded too, so a commit adding
a previously missing optional header can invalidate the snapshot.
Reuse checks those repository commits without rescanning or hashing headers.
Any recorded repository commit change triggers refresh. Different compiler
arguments or compiler versions select a different entry.

Uncommitted source/header edits, generated-file changes, and changes to headers
outside tracked repositories do not invalidate an existing entry. A cached AST
retains the source buffers captured when it was created, so its source text
and locations stay consistent. Disable caching to analyze working-tree edits.
Sources without a tracked Git commit use normal preprocessing and parsing.

Invalid or unreadable entries fall back to parsing. Failed parses are never
published. Cache storage failures do not fail an otherwise successful command.
Import still saves dependency metadata when the AST directory is unavailable.
Time-dependent builtin macro values follow the saved AST until its commit
changes. Compiler diagnostics are produced when a TU is parsed.
Concurrent access uses a per-entry lock and atomic file replacement; a busy
entry falls back to parsing without waiting.

## Code ownership

`config/ConfigurationAstCache*` owns YAML validation, precedence, and paths.
`tooling/astcache` owns preprocessing collection, commit validation, AST
serialization, and include reconstruction. `storage/astcache` owns relational
metadata and atomic database publication. Plain cache records live in
`model/AstCache.h`. Command adapters select and consume results without owning
cache formats. Native E2E BDD scenarios under `tests/e2e/features/ast_cache*`
exercise separate processes against the real executable and SQLite outputs.
