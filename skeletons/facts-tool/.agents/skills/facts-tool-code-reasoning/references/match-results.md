# Match results and invocation evidence

Use this workflow when the exact result set from one native matcher invocation
matters. First verify the selected binary and YAML-resolved pair:

```sh
facts-tool config show
facts-tool match --help
```

For an intentional store override, record the actual `--conf`/`--facts` pair
on the match command. `config show` reports configuration provenance and
defaults; it does not reflect overrides supplied to a later command. Native
matching validates the pair it actually receives.

The help output must advertise `--format json`; also verify the Python package
surface in the same environment:

```sh
python -c 'from facts_tool import MatchResults, load_match_results'
```

Check both capabilities in the selected environments; a newer checkout does
not establish which APIs an installed executable or wheel provides.

For a simple name or USR lookup, use the existing project discovery index
before reparsing:

```sh
facts-tool symbol find --name main
facts-tool symbol find --usr 'EXACT_USR'
```

Use `match` when an explicit AST predicate, a fresh occurrence set, an
expression, a call, or a relation is required. Select the smallest registered
translation-unit set that can answer that request:

```sh
facts-tool match --format json \
  --matcher 'functionDecl(hasName("main")).bind("symbol")' \
  src/main.cpp > matches.json
```

The default text format remains useful for a quick check and includes source
path, line, and column. JSON is the transient, exact collection for that
successful invocation; it is not a second database and does not replace the
persisted facts or project index.

Read saved output or captured stdout through the public SDK:

```python
from facts_tool import MatchResults, load_match_results

results = load_match_results("matches.json")
# The same model accepts captured native stdout:
# results = MatchResults.from_json(completed.stdout)
if not (results.complete and results.facts_committed and results.index_committed):
    raise RuntimeError("matcher result is not a complete successful publication")
for result in results:
    for binding_name, binding in result.bindings.items():
        print(result.translation_unit, binding_name, binding.usr)
        if binding.location is not None:
            print(binding.location.path, binding.location.line,
                  binding.location.column, binding.location.offset)
```

The binding map contains the names actually bound by the matcher (`symbol`,
`expression`, `call`, `callee`, `source`, `target`, and/or `site`); do not
invent absent bindings. Each match retains its selected `translation_unit`, so
the same header occurrence matched through two TUs remains two per-TU results.
Bindings expose a name/USR when available, an optional byte range, and either a
location or an explicit unavailable-location reason. A non-null location uses
the physical expansion path and coordinates, including macro expansions and
`#line` cases; line and column are one-based, while offsets are zero-based byte
offsets. A range, when present, is the half-open byte interval
`[offset, offset + size)`.

Keep three evidence layers distinct:

- Persisted facts include symbols plus supported relation, expression, and
  source evidence produced by native matcher persistence.
- The project matched-symbol index contains only eligible named symbols and
  can be queried with `symbol find`; expressions and locationless implicit
  nodes do not become ordinary discovery rows.
- The JSON document is the exact successful result set for this invocation,
  including bindings that are not eligible for that index.

`facts_committed`, `index_committed`, and `complete` describe the result
publication. A successful empty or index no-op result can still have the
commit flags set. `complete=true` means matching completed over the selected
translation units; it does not establish whole-project, body, outgoing-call,
or freshness coverage. A failed or cancelled command emits no successful JSON
document. Use its exit status and stderr for the failure; do not infer a
stronger atomicity guarantee for an index-write failure from these flags.
Unsupported implicit persistence is also a command failure, even if the
matcher reached other bindings first.

For call results, bind exactly `call` and `callee`; for arbitrary relations,
bind declaration `source` and `target`, optionally `site`, and pass
`--relation-kind`. A symbol-only match establishes identity and location, not
outgoing-call coverage. For persisted body, relation, or source evidence after
the match, reopen the paired `CodeBase` and use the public SDK; never query
either database directly.
