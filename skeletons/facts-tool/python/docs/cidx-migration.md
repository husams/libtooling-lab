# cpp-indexer compatibility matrix

Reference implementation: `/Users/husam/workspace/cpp-indexer/python/indexer/`
`queryplan.py`, `query.py`, `model.py`, and `entity_graph.py`. This package
copies the declarative design, not indexing APIs or cidx storage assumptions.

| cidx public surface | Disposition | facts-tool behavior |
|---|---|---|
| `start`, `codebase`, `symbol` | implemented | same immutable source model |
| `entity` source | adapted | persisted symbol domain |
| `nodes`, `view`, `where` | implemented | facts and project catalogs |
| `eq`, `ne`, `glob`, `in_list` | implemented | typed Python values |
| `all_of`, `any_of`, `not_` | implemented | three-valued composition |
| `exists`, `none`, `all` | implemented | bounded relation evidence |
| `at_least`, `exactly` | implemented | count with unknown handling |
| target-set constructors | implemented | USR/name/spelling targets |
| `out`, `in_` | implemented | all 23 stored relations |
| `sites` | implemented | full relation-site keys |
| set operations | implemented | compatible logical domains |
| `select`, `distinct`, `order_by` | implemented | deterministic rows |
| `limit`, `count` | implemented | honest truncation |
| `path`, `rank` | implemented | simple shortest witnesses |
| `reverse_type_use` | adapted | direct stored type use only |
| `canonical_json`, `plan_to_dict` | implemented | complete frozen IR |
| `validate`, `Executor.explain/run` | implemented | dual-DB metadata |
| CXQ parser | unsupported | not part of the Python contract |

Semantic helpers are implemented, with two deliberate corrections:
`has_member` combines `field_of` and `method_of`, while `has_template_arg`
uses supplied template values rather than treating instantiation itself as an
argument. `implements` uses persisted inheritance because facts-tool has no
separate interface edge.

| cidx graph/model surface | Disposition |
|---|---|
| get/find and edge/ref/site lookup | adapted to paired adapters |
| neighbors/walk/reaches, bases/subclasses/members | implemented |
| callers/callees, definitions, parameters/templates | implemented |
| typed `CodeBase`, entity/callable/method/record | implemented |
| lazy fluent entity query and plan lowering | implemented |
| local callback filters | implemented, never serialized |
| separate `entity_node/entity_edge` graph | `E_CAPABILITY` |
| normalized `type_node/type_layer/type_edge` | `E_CAPABILITY` |
| call-argument objects | `E_CAPABILITY` |
| inferred devirtualization from type layers | `E_CAPABILITY` |

Two visible layout/API adaptations are deliberate. The reference repository's
BDD layout is `python/tests/e2e/features/steps`; this package uses the ordinary
pytest-bdd layout `python/tests/bdd/features` plus `python/tests/bdd/steps`.
The reference convenience form `cb.find(pattern)` is represented by exact
`CodeBase.get(ref)` lookup and declarative glob search:

```python
start(codebase()) | nodes(glob("qualified_name", pattern))
```

`symbol(ref)` seeds exact USR matches first, then every exact qualified-name
match, then every exact short spelling match. Multiple matches are retained in
ascending persisted identity order for deterministic template queries.

facts-tool physical template names are reversed from standard/cidx wording.
Public `template_parameter` reads physical `template_argument` (declared slot),
and public `template_argument` reads physical `template_parameter` (supplied
value). Indexing, import, extraction, resolve, relink, migration, backfill, and
all mutation APIs are intentionally absent from the package exports.
