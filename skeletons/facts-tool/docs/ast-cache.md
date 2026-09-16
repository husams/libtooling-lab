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
| `extract` | Saves parsed TUs and traverses loaded ASTs on subsequent extraction. |
| `match` | Runs the matcher against the shared saved AST. |
| `analyse variable-flow` | Builds its analysis from saved ASTs. |
| `analyse call-graph --recover-missing` | Reuses saved ASTs for recovery scans. |
| `import` | Reuses a saved AST's include records when available. |
| `analyse dependency` | Reuses a saved AST's include records when available. |
| Queries and catalog operations | Do not load or create ASTs. |

Import and dependency discovery retain preprocessing-only behavior on a miss.
They can therefore still accept code whose preprocessing succeeds but whose
C++ semantic analysis would fail. Commands that build an AST populate the
shared cache. Existing extraction freshness checks still apply: use `extract
--force` to request extraction even when the facts database is already current.

Verbose output (`-v 1`) identifies `ast-cache: miss`, `ast-cache: stored`, and
`ast-cache: hit`. Disabled caching performs no cache reads or writes.

## Boundaries

The cache stores Clang's serialized AST, separately from the project registry
and facts databases. It does not replace either database or cache a command's
query result. AST extraction, match selection, and analysis still run on a hit.
Extraction consumes one TU at a time to avoid retaining a project's entire
AST collection in memory.

The cache key includes the source, compilation directory, compiler version,
effective command arguments, response-file contents, and relevant environment.
When import expands a response file into stored command arguments, rerun
`import` after changing that file so subsequent commands use the updated arguments.
Input content digests detect source/header changes, including edits that retain
both file size and modification time. Include lookup validation also detects
new headers that change an include's resolution or a `__has_include` result.

Invalid or unreadable entries fall back to parsing. Failed parses are never
published. Cache storage failures do not fail an otherwise successful command.
Translation units with compiler warnings, time-dependent builtin macros, or
imported Clang module files are reparsed. Their diagnostics, generated values,
and module dependencies therefore remain current.
Concurrent access uses a per-entry lock and atomic file replacement; a busy
entry falls back to parsing without waiting.

## Code ownership

`config/ConfigurationAstCache*` owns YAML validation, precedence, and paths.
`tooling/astcache` owns compiler-input identity, validity, AST serialization,
and include reconstruction. Command adapters select and consume ASTs without
owning cache formats. Native E2E BDD scenarios under `tests/e2e/features/ast_cache*`
exercise separate processes against the real executable and SQLite outputs.
