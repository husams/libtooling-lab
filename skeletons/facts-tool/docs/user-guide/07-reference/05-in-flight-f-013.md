# In flight: F-013 (as of 2026-09-08)

**Everything under S-030 and S-031 in this chapter is NOT ON MAIN.** It
describes open pull requests, not shipped behavior. Do not use anything
below as a reference for what `facts-tool` currently does - for that, see
the rest of this guide. This chapter exists so a reader tracking the
project's direction knows what's coming and can review the linked PRs.

F-013, "Agent-ready facts-tool SDK aligned with native facts and source
evidence," is **In Progress**.

| Story | Backlog status | PR | On main? |
|---|---|---|---|
| S-029 | Done | [#76](https://github.com/husams/libtooling-lab/pull/76) | **Yes - merged** |
| S-030 | Accepted | [#77](https://github.com/husams/libtooling-lab/pull/77) | **Not on main** |
| S-031 | Needs Work | [#78](https://github.com/husams/libtooling-lab/pull/78) | **Not on main** |
| S-032 | Ready, not started | - | Not started |

## S-029 - shipped (merged)

Native schema-12 read-only persisted call-graph run APIs: run discovery/get,
paged roots, edges with semantic kind and sites, targets/path outcomes,
frontier/boundaries, and recovery diagnostics. This is documented as current
behavior throughout this guide - see
[persisted call-graph runs](../05-python-sdk/06-persisted-callgraph-runs.md)
for the full reader API, and [storage schema](02-storage-schema.md) for the
underlying schema-12 tables.

## S-030 - **Not on main** - native opt-in expression/field-access/source evidence

PR [#77](https://github.com/husams/libtooling-lab/pull/77), branch
`codex/s-030-expression-evidence`. Bumps the fresh-schema `user_version`
from 12 to **13**.

- **New CLI surface**: `match --matcher '...bind("expression")'
  --capture-source`. Binding `"expression"` requires the bound node be an
  `Expr`, and `--relation-kind` is rejected for that binding - it is
  mutually exclusive with the shipped `symbol`/`call`+`callee`/
  `source`+`target`[+`site`] bindings. `--capture-source` is a separate flag
  that persists exact byte-range definitions for matched functions/
  methods/records.
- **New tables (schema 13)**: `expression_occurrence` (one row per
  source-fingerprinted occurrence: nearest owning callable, resolved target
  when known, `expression_kind`, `access`, byte range, SHA-256,
  `freshness ∈ {current, stale, unavailable}`) and `source_region` (one row
  per captured definition: symbol, file range, its own SHA-256,
  `symbol_kind`, freshness). Both are append-only by versioned identity -
  changed source creates a new evidence row rather than overwriting.
- **Access classification** for a matched expression's effect on a field:
  a direct field write -> `write`; compound-assign/increment/decrement ->
  `read_write`; plain value use -> `read`; address-of a field -> `escape`;
  a call through a non-const reference parameter -> `escape`; an indirect/
  unresolved call argument -> `unknown`; a non-field expression -> `none`.
  Parentheses and implicit casts are transparent (inherit the wrapped
  expression's classification). Explicitly **not fabricated**:
  alias-mediated effects, and macro/dependent/system-header/invalid ranges
  (persisted as `unknown`/`unavailable` with a reason string, never
  guessed).
- SIGINT during `match` on this branch reports `facts-tool: cancelled during
  match`, exit 130, rolling back the active evidence transaction while
  keeping prior committed rows.

## S-031 - **Not on main** - Python SDK surface for the above

PR [#78](https://github.com/husams/libtooling-lab/pull/78), branch
`codex/s-031-sdk-source-regions`, Backlog status **Needs Work** (open review
concerns exist that are not enumerated here - see the live PR for specifics).

- `schema.py` on this branch accepts `user_version` 13 in addition to
  10/11/12, and validates a new evidence-tables set and their required
  columns when the version is 13.
- **New `CodeBase.evidence` facade** plus convenience methods directly on
  `CodeBase`: `expressions()`/`expression_occurrences()`,
  `field_accesses(ref)`, `field_writers(ref)`/`field_writes(ref)` (only
  proven `write`/`read_write` rows - `unknown`/`escape` rows are visible
  only via `field_accesses` and set `result.unknown`),
  `source_regions(ref, include_text=, max_bytes=, limit=, after_id=)`
  (aliases: `source_sections`, `definition_regions`), and
  `ancestors(ref, max_depth=1)` (a thin wrapper over `graph.bases`).
- `source_regions` hashes the on-disk file in streaming chunks and seeks
  only the requested byte range; it never returns guessed text - a changed
  fingerprint, truncated file, missing file, unreadable file, or invalid
  UTF-8 range all downgrade `freshness` to `stale` or `unavailable` with an
  explicit reason string rather than silently substituting stale content.
  `include_text=True` with a region larger than `max_bytes` raises
  `E_LIMIT`.
- **New/reused error codes**: `E_CAPABILITY` (calling any evidence method
  against schema 10–12), `E_IDENTITY` (ambiguous symbol ref), `E_LIMIT`
  (bad page size or byte bound).
- The PR description self-reports validation results (unit/BDD/installed-
  wheel/distribution checks); this guide did not independently re-verify
  those numbers, and Backlog's **Needs Work** status means at least one
  open review concern exists beyond what's summarized here.

## S-032 - not started

Backlog: "Ready, not started." Planned scope (per the wiki planning page for
this work): concise, isolated-agent acceptance criteria for the installed
CLI/SDK, blocked on S-031 landing first. No code or documentation exists for
this story yet.

## Before you rely on any of this

Schema and API surfaces described above can change before merge, and S-032
has not even started. If you're reading this guide after S-030/S-031 have
landed, re-check [storage schema](02-storage-schema.md) and
[persisted call-graph runs](../05-python-sdk/06-persisted-callgraph-runs.md)
directly against the installed `facts-tool` build and SDK rather than
trusting this chapter's schema-13 description as current.
