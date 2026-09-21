# Typed matcher results and invocation evidence

Use `client.matches.create(...)` and retain its returned job ID, expression,
selection, traversal mode, and role mappings. See
[matching and bindings](how-to-search-symbol.md) for request examples.
Wait for that job before reading its results:

```python
summary = job.wait(timeout=120)
print(job.id, summary.coverage, summary.match_count, summary.index_revision)
for diagnostic in client.matches.diagnostics(job.id):
    print(diagnostic.severity, diagnostic.message)
for row in client.matches.results(job.id):
    for binding_name, binding in row.bindings.items():
        print(row.translation_unit, binding_name, binding.node_kind, binding.usr)
        if binding.location is not None:
            location = binding.location
            print(location.path, location.line, location.column, location.offset)
        else:
            print(binding.location_unavailable_reason)
        if binding.range is not None:
            print(binding.range.path, binding.range.offset, binding.range.size)
```

Use these typed `MatchRow` records for the exact successful invocation.
Do not capture stdout, parse command completion text, reconstruct results from
the global index, or pass a REST result to the legacy `MatchResults.from_json`
reader. The v2 `MatchResult` summary uses `coverage`, counts, and
`index_revision`; it has no `complete`, `facts_committed`, or
`index_committed` fields from the CLI JSON format.

`summary.matches is None` means records have not been fetched, not that there
were no matches. `client.matches.results(job.id)` lazily fetches all pages as
needed. Use `.collect()` for deliberate eager materialization.

Preserve the actual binding names, including arbitrary/helper names and the
automatic `root` for expressions without explicit binds. Keep
`translation_unit`: one header occurrence matched through two TUs is two
per-TU results. Keep each row's optional `relation_kind`.

Bindings expose node kind, optional name/USR, optional location/range, and
`location_unavailable_reason`. Locations are physical expansion coordinates;
line/column are one-based and offsets are zero-based bytes. A range is
`[offset, offset + size)`. Do not invent absent locations or source text.
`capture_source=True` requests source capture, but the current typed
`MatchBinding` has no source-text member; report a capability gap if the
requested answer requires text unavailable through the chosen API.

Keep three evidence layers distinct:

- The job's result rows are the exact invocation evidence.
- Persisted facts contain the supported symbols, expressions, and relations.
- The global symbol index provides reusable identities across registered
  facts, including extraction updates; it is not the invocation result set.

Check job failure/cancellation before using evidence. A failed multi-file job
is not evidence of success or of global rollback; do not assume all earlier
server-side work was undone. Successful empty output is valid.
`coverage="complete"` covers the selected matcher operation, not the whole
project, all function bodies, or all outgoing calls. Re-run when current source
is required and the previous job predates a relevant edit.
