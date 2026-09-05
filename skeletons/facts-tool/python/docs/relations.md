# Relation catalog

Stored relations use source-to-destination direction unless `in_` or
`inbound=True` reverses traversal.

| ID | Python name | Meaning |
|---:|---|---|
| 1 | `calls` | caller to callee |
| 2 | `inherits` | derived record to base |
| 3 | `contains` | lexical scope to declaration |
| 4 | `specializes` | specialization to template |
| 5 | `instantiates` | instance to template |
| 6 | `overrides` | overriding to overridden method |
| 7 | `uses` | referencing to referenced symbol |
| 8 | `field_of` | field to owning record |
| 9 | `method_of` | method to owning record |
| 10 | `construct_value` | owner to value construction target |
| 11 | `construct_temp` | owner to temporary construction target |
| 12 | `construct_heap` | owner to heap construction target |
| 13 | `construct_copy` | owner to copy construction target |
| 14 | `construct_move` | owner to move construction target |
| 15 | `factory_construct` | factory to constructed target |
| 16 | `destroy` | owner to destroyed target |
| 17 | `friend` | granting record to friend |
| 18 | `dispatch_calls` | call owner to inferred override target |
| 19 | `alias_of` | alias to resolved declaration |
| 20 | `of_type` | declaration to declared type |
| 21 | `return_type` | callable to returned type |
| 22 | `param_type` | callable to parameter type by position |
| 23 | `template_argument_type` | instance to supplied type by position |

Hyphenated persisted spellings are accepted aliases. SDK-only joins are
`has_parameter`, `has_template_parameter`, `has_template_argument`,
`includes`, `definition`, and `declaration`. These read existing side tables;
they do not materialize new edges.

Edge rows preserve source, destination, kind, position, access, virtual-base,
implicit, lexical, and count fields. Site rows preserve the complete edge key,
file, line, column, offset, receiver type, and certainty.
