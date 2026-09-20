# Results, budgets, and failures

`Result.shape` is `nodes`, `rows`, `scalar`, or `path`; matching convenience
properties are `nodes`, `rows`, `scalar`, and `paths`. Iteration yields non-
scalar values. `to_dict()` and `to_json()` retain ordering, view, cursor,
`truncated`, `partial`, `unknown`, and both database identities for every
shape, including empty and scalar results.

## Lazy execution and lifetime

`open_codebase(..., lazy=True)` is the default. `executor.run(plan)` and fluent
`query.run()` return deferred results; iteration pulls rows as needed. A plain
`for row in result` loop does not cache rows. The codebase must remain open
until iteration or materialization finishes. Explicitly close a retained
iterator when abandoning it early to release its active SQLite cursor.

Set `lazy=False` on either `run` method to execute immediately, or on
`open_codebase` to set the default for all plan queries. A per-query `lazy=True`
overrides an eager codebase. Eager results can be read after the codebase closes.

`values`, `nodes`, `rows`, `paths`, `len(result)`, `list(result)`, `to_dict()`, `to_json()`, and
`result.materialize()` collect and cache the full bounded result. Metadata such
as `truncated`, `partial`, `unknown`, `cursor`, and `scalar` needs completed
execution; reading it before exhaustion materializes the result. After complete
streaming iteration, metadata is available without another query.

An unmaterialized result is repeatable: each iteration executes its plan again.
Materialization after partial or complete streaming also executes it again and
caches that full pass. Use eager mode when repeated reads must use the same
snapshot. Each pass observes the database through the existing SQLite connection.
Resolved FileId paths are cached for the codebase's lifetime. Close and reopen
the codebase after project registry edits, such as moving files or changing
the active clone, to refresh paths in subsequent results.

Enumeration, filtering, projection, and limits stream where supported. Sorting
and distinct stages need an intermediate collection; counts consume their input
before returning a scalar. Traversal, path, and set-operation plans currently
defer execution but retain their existing bounded materialization. Specialized
evidence and persisted callgraph APIs keep their existing eager page behavior.

Plan validation errors still occur at `run`. Errors that require database rows,
such as a missing symbol or unknown evidence, occur during consumption in lazy
mode. Place exception handling around iteration or use `lazy=False`.

Defaults are 10,000 enumerated rows, 10,000 traversal states, 1,000 results,
depth 32, 10,000 path expansions, and 200,000 witness reconstructions. Supply
`Budgets` to `open_codebase` for stricter limits and `result_cap` per run. A hit sets
`truncated`; a truncated count returns `None` so it cannot look exact. An early
`limit` never increases later traversal or final caps.
For non-symbol streaming views, an early `limit` can stop before enumeration
reaches its budget; truncation then reflects the work actually evaluated.

Lazy mode preserves these caps. For more than 1,000 results, increase both
relevant limits, for example `Budgets(enumeration=100_000, result_cap=100_000)`
passed to `open_codebase`. Iteration streams decoded rows within those bounds.

`after_id` applies to the first enumeration and `Result.cursor` identifies the
last returned item when a result cap truncates a page. Symbol/file numeric
cursors are stable only within the reported database identity; compound edge
and side-table logical IDs are domain strings.
ID-based continuation is intended for the default enumeration order; custom
sorting does not provide a matching sort-key continuation cursor.

All public failures are `FactsToolError` with stable `code` and `message`:

| Code | Meaning |
|---|---|
| `E_SOURCE` | missing source |
| `E_VIEW`, `E_FIELD`, `E_KIND` | invalid catalog input |
| `E_RELATION`, `E_DEPTH` | invalid graph request |
| `E_LIMIT`, `E_BUDGET` | invalid or exhausted bound |
| `E_SETOP`, `E_STAGE` | incompatible plan shape/order |
| `E_UNKNOWN` | requested error on incomplete evidence |
| `E_CAPABILITY` | semantics unavailable from stored facts |
| `E_DATABASE`, `E_DATABASE_ROLE` | open or role failure |
| `E_SCHEMA`, `E_DATABASE_PAIR` | incompatible schema/pair |
| `E_IDENTITY` | invalid packed or FileId mapping |

SQL-like and Unicode strings remain bound values. No textual CXQ parser is
provided; the Python constructors are the complete supported grammar.
