# Native matcher results

`facts_tool.MatchResults` reads the JSON emitted by the native `facts-tool
match` command. It is a read-only reader for a saved result document; it does
not invoke facts-tool, open a database, or write facts.

```python
from facts_tool import MatchResults, load_match_results

results = load_match_results("matches.json")
for result in results:
    print(result.translation_unit, result.relation_kind)
    for binding_name, binding in result.bindings.items():
        print(binding_name, binding.node_kind, binding.name, binding.location)
```

`MatchResults.matches` is a tuple, and each result's `bindings` mapping is
immutable. `len(results)` counts match rows. `to_dict()` and `to_json()` retain
the native schema, including the completion and commit flags. `from_json()`
accepts `str` or UTF-8 `bytes`; `from_dict()` accepts a mapping.

Each `MatchBinding` contains the native `node_kind`, optional `name` and `usr`,
optional `location`, optional `range`, and an optional
`location_unavailable_reason`. Locations use one-based line and column values;
offsets and range sizes are zero-based byte counts. A match's
`translation_unit` identifies the selected absolute source, while a binding's
location path can identify a shared header occurrence in that TU.

Schema version 1 validates required fields, value types, absolute paths, and
non-negative coordinates; unknown additive fields are ignored. Invalid JSON,
unsupported versions, and malformed values raise `FactsToolError` with code
`E_SCHEMA`. File read failures raise `E_SOURCE`.

`facts_committed` and `index_committed` describe successful publication (including
an empty no-op); they do not mean every binding became a discovery-index row.
The index accepts eligible named symbols. Successful expression bindings can
appear in results without becoming index rows; skipped-index diagnostics remain
on stderr. Unsupported implicit symbol persistence still fails the invocation
and produces no successful result document.
