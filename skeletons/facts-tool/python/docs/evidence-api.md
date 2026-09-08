# Expression and bounded source evidence

Schema 13 adds opt-in native evidence without changing ordinary extraction.
Open a paired database normally, then use the read-only `CodeBase.evidence`
facade (the convenience methods on `CodeBase` have the same behavior):

```python
with open_codebase(facts_db=facts, project_db=project) as codebase:
    writes = codebase.field_writers("app::Box::value")
    uses = codebase.field_accesses("app::Box::value")
    section = codebase.source_regions("app::run", include_text=True,
                                      max_bytes=32_000)
```

`expression_occurrences` accepts an owning symbol, target symbol, access
classification, deterministic `limit`, and an `after_id` cursor. `field_writers`
returns only proven direct `write` and `read_write` rows; `unknown` and `escape`
rows remain visible through `field_accesses` and set the result's `unknown`
flag. Alias-mediated writes are never fabricated.

`source_regions` returns the native symbol identity, file path, UTF-8 byte
offset and size, lowercase SHA-256, symbol kind, freshness, and an optional
bounded `text` value. The SDK hashes the file in streaming chunks and seeks
only the requested range. A changed, truncated, missing, invalid-UTF8, or
unavailable source is returned with `stale` or `unavailable` freshness and a
reason; no guessed text is returned. The resolved file path must match the
capture path recorded in `facts_project_provenance`, so an identical file in
another checkout is unavailable rather than falsely current. `limit` and
`after_id` apply stable numeric paging, while `max_bytes` bounds each
requested region.

Evidence methods raise `E_CAPABILITY` for schema 10–12, `E_IDENTITY` for an
ambiguous symbol, and `E_LIMIT` for invalid page or byte bounds. Existing graph
and callgraph methods remain available on legacy schemas.
