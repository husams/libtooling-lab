# Language: sources and predicates

`start(source)` creates a frozen `Query`; `query | stage` returns a new query
and leaves the reusable prefix unchanged. Sources are `codebase()`,
`symbol(ref)`, and `entity(ref)`. `entity` is adapted to the persisted symbol
domain because facts-tool has no separate entity graph.

`nodes(predicate=None, unknown="exclude")` enumerates the selected view.
`where(predicate, unknown="exclude")` filters existing nodes. The comparison
constructors are `eq(field, value)`, `ne`, `glob` using shell-style matching,
and `in_list(field, values)`. Values are bound data; field names come from the
catalog and cannot become SQL.

Boolean constructors are `all_of`, `any_of`, and `not_`. Empty `all_of(())` is
true and empty `any_of(())` is false. Stored null is a known value and differs
from absent evidence. Unknown results are excluded, included, or rejected with
`E_UNKNOWN` according to the stage policy; `Result.unknown` records that the
run encountered one.

Relationship quantifiers are:

```python
exists("calls", eq("name", "save"))
none("overrides")
all("calls", eq("kind", "function"))
at_least(2, "calls", max_depth=3)
exactly(1, "calls")
```

Each accepts a target predicate, a depth window, and `inbound=True`. Empty
universal sets are true. Truncated or incomplete evidence produces unknown
when the answer cannot be proved.

Target sets are `any_target(refs)`, `all_targets(refs)`, and `no_targets(refs)`.
They compose with `inherits_from`; semantic helpers also include `implements`,
`has_ancestor`, `has_member`, `has_method`, `has_field`, `has_nested`,
`has_template_arg`, `is_specialization_of`, `is_instantiation_of`, `calls`,
`called_by`, `uses`, `used_by`, `is_abstract`, `is_interface`, `is_pure`,
`is_static`, `is_template`, and `is_instance`.
