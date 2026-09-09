# Installed agent workflows

Use YAML-resolved paired paths and the installed tools in every workflow. Start by
checking `facts-tool config show` and the relevant command help, then reuse the
same project/facts pair for native and SDK calls. Do not require explicit
database-path flags; use `--config FILE` to select another YAML file when needed. Keep temporary acceptance
stores outside the checkout; do not set `PYTHONPATH`, mutate global stores, or
read SQLite directly.

## Build or reuse facts

```sh
facts-tool import -p build
facts-tool extract
facts-tool analyse call-graph \
  --function app::run  # capture the printed run_id
```

The native command output identifies the persisted graph run. The SDK reads
that exact run without replaying traversal. Obtain `facts_path` and
`project_path` from the configured pair as described in the
[SDK query guide](query-cpp.md); the SDK still takes concrete path arguments:

```python
from facts_tool import open_codebase

with open_codebase(facts_db=facts_path, project_db=project_path) as cb:
    run_id = 1  # parsed from the native completion line
    run = cb.callgraphs.get(run_id)
    edges = run.edges
    print("graph: " + ", ".join(
        f"{edge.source.qualified_name}->{edge.target.qualified_name}"
        for edge in edges
    ))
```

## Answer focused questions

Use native symbol discovery first, selecting an exact USR when a name is
ambiguous; use `match` only for missing symbol evidence. Use the SDK for typed
navigation (`callees`, `callers`, `bases`), schema13 expressions and field
effects, and exact bounded definition regions:

```python
with open_codebase(facts_db=facts_path, project_db=project_path) as cb:
    print("symbols: " + cb.find("app::run").name)
    print("writers: " + str(len(cb.field_writers("app::Box::value").rows)))
    print("ancestors: " + ", ".join(
        item.qualified_name for item in cb.ancestors("app::Box")
    ))
    region = cb.definition_regions(
        "app::run", include_text=True, max_bytes=32_000
    )
    print("source: " + region.rows[0]["freshness"])
```

Check `truncated`, `partial`, `unknown`, `provenance`, and each row's
`freshness` before stating a conclusion. A complete stored graph is not proof
of complete source extraction; a limited match is not proof of full-file
coverage. Changed, missing, invalid, cross-checkout, macro, dependent, and
unavailable regions stay typed and reasoned.

## Acceptance discipline

An isolated acceptance harness should invoke each requested query once, reuse
the open SDK session, and report one concise sentence per query. Record native
tool-call count, captured output characters, and an approximate output-token
count when available. Run both wheel and source-distribution installs in clean
environments and verify imports resolve from the environment, not the
checkout. S-028 remains the owner of its skill-refinement acceptance.
