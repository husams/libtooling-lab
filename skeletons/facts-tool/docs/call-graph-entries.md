# Function entries in the shared call graph

A function entry records a successfully committed collection of that function's
call evidence. All entries refer to the existing shared symbol and relation
graph; asking for two roots does not store two copies of their shared callees.

```sh
facts-tool import --conf project.sqlite -p build
facts-tool extract --conf project.sqlite --output facts.sqlite source.cpp
facts-tool analyse call-graph-entry --conf project.sqlite --facts facts.sqlite \
  --function 'app::run' --format json
```

When changing imported commands, explicitly identify an existing facts store:

```sh
facts-tool import --conf project.sqlite --facts facts.sqlite -p build
```

Its entries are invalidated before the new project configuration commits; an
invalidation error stops the import. An existing nonempty project without an
explicit facts path or facts template rejects mutations with an actionable
error; initial empty-project setup remains allowed. Configured facts paths can also identify
the pair. Catalog mutations accept the same `--facts` option, including file compile-option
edits and repository, component, or directory changes. Listing and dry runs do
not invalidate entries. A per-source facts template resolves existing stores
from registered compile commands; a reimport invalidates all selected stores.
Facts output paths must differ from the project database, including aliases.

Exact qualified names or USRs select entries; `root-not-found` and
`ambiguous-root` errors exit 2, with candidate USRs for disambiguation.
The lookup opens existing stores read-only and does not generate evidence.

The JSON object includes `schema_version: 1`, decimal-string `symbol_id`, `usr`,
`entry_available`, `graph_node_ref`, `is_leaf`, `external_targets`, and `coverage`.

| Stored evidence | `entry_available` | `graph_node_ref` | `is_leaf` |
|---|---|---|---|
| Fully collected body with outgoing calls | true | symbol ID | false |
| Fully collected body without outgoing or unresolved calls | true | symbol ID | true |
| Known symbol without committed generation | false | null | null |

A narrow symbol or call match does not certify the caller's entire body. A
subsequent full extraction republishes completed entries, including leaves.

Entry availability is separate from freshness and transitive completeness.
With a validated project pair, `coverage` reports the selected node's metadata,
freshness and action; `extraction_coverage` summarizes its reachable graph.
Without a project, both remain `unknown`. Entries do not imply fresh complete
graphs; the existing [coverage contract](call-graph.md) remains applicable.

A known declaration-only call target keeps its canonical symbol identity and
exact call site in an external-reference record. Extracting its definition from
another registered component reuses that identity, retains callers and sites,
and removes the resolved external boundary. An indirect call has no guessed
target symbol ID and remains explicitly unresolved.

Numeric IDs are scoped to a validated project/facts pair. An incompatible pair
reports `incompatible-symbol-universe`; independently imported databases must
not be joined by their numeric IDs. Compatible sources are registered and
processed through the selected project's existing file identity mapping.

Facts provenance records the canonical registered path and semantic-universe
key for file IDs. This explicit identity evidence is separate from entries and
from the match-only symbol index. A changed or incompatible mapping requires a
fresh facts rebuild rather than silently reinterpreting stored numeric IDs.
Legacy facts without provenance can still be queried with unknown identity
coverage; adding facts requires a fresh store when the old pairing cannot be
proved. A schema migration alone does not establish that pairing.

## Storage and lifecycle

Facts schema 11 declares `facts_project_provenance(file_id, path, universe_key)`
in both fresh creation and migration, and adds `callgraph_entry(symbol_id, graph_node_ref)` and
`callgraph_external_reference(source_id, destination_id, kind, position,
file_id, offset, external_symbol_id)`. The entry fields reference `symbol(id)`
and must be equal. The external ID equals `destination_id`; its other six
fields reference the corresponding `relation_site` key. Deletion cascades
prevent dangling references. No per-root graph table is introduced.

Observed indirect calls are kept separately as source call sites in
`callgraph_unresolved_site(source_id, file_id, offset, line, col)`. This evidence
has no destination field and cannot invent an external target. Regeneration
replaces prior call and unresolved-site evidence for the collected bodies.

Migration from schema 10 preserves existing symbols, IDs, relations, and sites.
New evidence tables start empty; migration alone cannot prove generation or
pairing. Unsupported newer facts versions reject writes.

Generation and entry publication share the facts transaction. Failure restores
the previous committed entry state and its provenance. Dependency facts use the
same invalidation and rollback contract. Supported mutations conservatively
invalidate every entry in the selected facts store and republish only fully
collected callable bodies. Library-only extraction or a zero-match write also
clears caller entries; extract all desired source files together to republish
them. Provenance registration scans stored fact tables and external-reference
registration scans call sites, so write cost grows with the shared store.

The project matched-symbol index retains exactly its four columns. Only match
populates it; full extraction and entry generation do not maintain that index.
