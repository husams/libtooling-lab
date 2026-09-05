# Results, budgets, and failures

`Result.shape` is `nodes`, `rows`, `scalar`, or `path`; matching convenience
properties are `nodes`, `rows`, `scalar`, and `paths`. Iteration yields non-
scalar values. `to_dict()` and `to_json()` retain ordering, view, cursor,
`truncated`, `partial`, `unknown`, and both database identities for every
shape, including empty and scalar results.

Defaults are 10,000 enumerated rows, 10,000 traversal states, 1,000 results,
depth 32, 10,000 path expansions, and 200,000 witness reconstructions. Supply
`Budgets` to `open_codebase` for stricter limits and `result_cap` per run. A hit sets
`truncated`; a truncated count returns `None` so it cannot look exact. An early
`limit` never increases later traversal or final caps.

`after_id` applies to the first enumeration and `Result.cursor` identifies the
last returned item when a result cap truncates a page. Symbol/file numeric
cursors are stable only within the reported database identity; compound edge
and side-table logical IDs are domain strings.

All public failures are `FactsToolError` with stable `code` and `message`:

| Code | Meaning |
|---|---|
| `E_SOURCE` | missing or ambiguous source |
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
