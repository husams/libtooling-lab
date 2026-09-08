# Views and catalog

Every query has a **current view** - the shape of node the query is
enumerating. `view(name)` switches it; `select(fields)`, `where(pred)`, and
the relation-taking stages all validate field and relation names against
the view's catalog. This chapter documents the two catalogs (facts and
project), the raw symbol-kind mapping, and the relation catalog including
its SDK-only pseudo-joins.

## Facts views vs. project views

```text
Facts views:   symbol, parameter, template_parameter, template_argument,
               edge, site, definition, enumeration, enumerator,
               initializer, return_type
Project views: repository, clone, component, directory, file
```

`view(name)` fails `E_VIEW` for any name outside these two lists:

```python
from facts_tool.queryplan import start, codebase, view, nodes

start(codebase()) | view("not_a_view") | nodes()
```

```text
E_VIEW: unknown view 'not_a_view'
```

An unlisted field on `select`/`order_by`/predicates fails `E_FIELD` against
the *current* view, even on a query that would return zero rows:

```python
start(codebase()) | select(("not_a_field",))
```

```text
E_FIELD: unknown symbol field(s): not_a_field
```

### Symbol view

Symbols expose identity, USR, qualified and short names, the raw `kind_id`
and mapped `kind`, node kind, declaration `FileId`/path/line/column/offset,
access, properties, return spelling, and every stored qualifier:
definition, implicit, static, virtual, const, volatile, inline, pure,
override, linkage/external, variadic, deleted/defaulted/explicit/final,
abstract, polymorphic, constexpr kind, noexcept, and reference qualifier.

### Parameter view

Parameters expose owner, source order, name, packed type identity,
location and region, pointer/reference/forwarding-reference/const/pack
flags, and `has_default`, expression, evaluated kind, and evaluated value.

### Template views - reversed vocabulary, verify before writing docs

facts-tool's physical template table names are **reversed** from standard
and cidx terminology, and this is easy to get backwards when reading the
storage schema. The public `template_parameter` **view** reads the physical
`template_argument` table; the public `template_argument` **view** reads the
physical `template_parameter` table
(`view_parameters.load_template_parameters`/`load_template_arguments`).

The two **view** names, and therefore the two pseudo-relations you traverse,
mean what you would expect: `has_template_parameter` gives you declared
slots and `has_template_argument` gives you supplied values. Only the
underlying table names are swapped. Verified against a real database holding
`app::Box<T, int... Args>` and its instantiation `app::Box<int, 7>`:

```python
params = (
    start(symbol("app::Box")) | out("has_template_parameter")
    | select(("name", "is_parameter_pack", "position"))
)
args = (
    start(symbol("app::Box")) | out("has_template_argument")
    | select(("value", "is_pack", "position"))
)
```

```text
[{'name': 'T', 'is_parameter_pack': False, 'position': 0},
 {'name': 'Args', 'is_parameter_pack': True, 'position': 1}]
[{'value': '', 'is_pack': False, 'position': 0},
 {'value': '7', 'is_pack': True, 'position': 1}]
```

The two views carry disjoint field sets, which is the fastest way to tell
which one you are on: `template_parameter` rows have `name`,
`is_parameter_pack`, `is_non_type`, and `is_template_template`;
`template_argument` rows have `value`, `is_pack`, `kind`, and `pack_index`.
Asking for the wrong field fails `E_FIELD`, for example
`E_FIELD: unknown template_parameter field(s): is_pack, value`.

Both queries above start from the single ref `"app::Box"`. Clang persists
the primary template *and* its instantiation under the same
`qualified_name`, `"app::Box"`, so that one ref resolves to two symbol rows
(ids `8589934595` and `8589934602` in this database) and the declared slots
and the supplied values come back from the appropriate one. There is no
angle-bracketed ref to reach the instantiation directly: both
`symbol("app::Box<int>")` and `symbol("app::Box<int, 7>")` fail `E_SOURCE`
against a real Clang-parsed database. The `"app::Box<int>"` spelling exists
only inside the SDK's own synthetic BDD fixture, where it is a hand-picked
test id rather than a Clang name; guide examples borrowed from
`examples/facts.py` need that ref replaced with `"app::Box"` before they
will run against real output.

### Definition view

Definitions expose their **own** `FileId`/path, `offset`, and `size` - not
line/column. Verified live:

```python
defs = cb.graph.definitions("app::run")
print(dict(defs[0]))
```

```text
{'symbol_id': 8589934612, 'file_id': 1, 'offset': 182, 'size': 75,
 'id': 'definition:8589934612', 'file': '.../source.cpp',
 '_key': 'definition:8589934612', '_view': 'definition'}
```

`_key` and `_view` are the SDK's internal row bookkeeping. They are present
on raw rows and stripped by `Entity.to_dict()`; ignore them in your own
output.

If you need a line/column for a definition, resolve it from the `symbol`
view's declaration coordinates instead, or from a `relation_site`/edge
`sites()` row for a call into that function.

### Enumeration / enumerator / initializer / return_type views

Enumerations expose underlying type, scoped, and fixed flags; enumerators
expose value and written initializer. Initializers and parameter defaults
retain both written and evaluated forms. `return_type` is a side-table view
mirroring a callable's `return_type_spelling` field.

### Project views

Repositories expose kind, remote, active clone, and semantic universe;
clones expose repository/path/label; components expose path/version/
ownership; directories expose component-relative paths. Files expose
resolved path, hash, mtime, driver, compile options, working directory,
indexed state/time, and override state. A file that was only ever seen via
`#include` (never compiled directly) has `driver: null` and
`compile_options: null`:

```python
files = start(codebase()) | view("file") | nodes()
files |= select(("id", "path", "driver", "compile_options"))
```

Real output against a one-TU project (`source.cpp` compiled directly,
`api.hpp` only included):

```text
{'id': 1, 'path': '.../source.cpp', 'driver': '/opt/homebrew/opt/llvm/bin/clang++',
 'compile_options': '["--driver-mode=g++","-std=c++20","\\u001ffacts-tool-source-argument"]'}
{'id': 2, 'path': '.../api.hpp', 'driver': None, 'compile_options': None}
```

## Symbol kinds

`kind_id` is the raw LLVM 22 `clang::index::SymbolKind` integer (0–31),
mapped by `catalog_kinds.symbol_kind` without shifting or inventing
categories:

| ID | `kind` | ID | `kind` |
|---:|---|---:|---|
| 0 | `unknown` | 16 | `enum_constant` |
| 1 | `module` | 17 | `instance_method` |
| 2 | `namespace` | 18 | `class_method` |
| 3 | `namespace_alias` | 19 | `static_method` |
| 4 | `macro` | 20 | `instance_property` |
| 5 | `include_directive` | 21 | `class_property` |
| 6 | `enum` | 22 | `static_property` |
| 7 | `struct` | 23 | `constructor` |
| 8 | `class` | 24 | `destructor` |
| 9 | `protocol` | 25 | `conversion_function` |
| 10 | `extension` | 26 | `parameter` |
| 11 | `union` | 27 | `using` |
| 12 | `type_alias` | 28 | `template_type_parm` |
| 13 | `function` | 29 | `template_template_parm` |
| 14 | `variable` | 30 | `non_type_template_parm` |
| 15 | `field` | 31 | `concept` |

Unknown future integer values render as `kind_N` so stored data stays
visible without inventing a category.

Distinct kinds observed in a real one-TU C++ project:

```text
{'unknown', 'function', 'instance_method', 'namespace', 'enum', 'constructor',
 'enum_constant', 'field', 'variable', 'struct', 'template_type_parm'}
```

## Relations catalog

Stored relations use source-to-destination direction unless `in_` or
`inbound=True` reverses traversal. There are 23 stored relations:

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

Hyphenated persisted spellings are accepted aliases (e.g. `"base"` for
`inherits`, `"dispatch-calls"` for `dispatch_calls`).

An unknown relation name fails `E_RELATION`, verified by using a
`queryplan.helpers` predicate name (`"has_field"`) where a *relation* name
was expected - `has_field` is a helper predicate, not a stored or
pseudo-joined relation:

```python
start(symbol("app::Box")) | out("has_field")
```

```text
E_RELATION: unknown relation 'has_field'
```

### SDK-only pseudo-joins

`out`/`in_` also accept a small set of SDK-only pseudo-relations that read
existing side tables directly and never materialize new edges:
`has_parameter`, `has_template_parameter`, `has_template_argument`,
`includes`, `definition`, `declaration`.

```python
params = start(symbol("app::run")) | out("has_parameter")
```

Real output (`app::run`'s parameter, `box`, with a default expression):

```text
{'name': 'box', 'has_default': True, 'default_expression': '= {}'}
```

`includes` walks a file's `#include` graph:

```python
includes = (
    start(codebase()) | view("file")
    | nodes(glob("path", "*source.cpp")) | out("includes")
    | select(("path",))
)
```

```text
[{'path': '.../api.hpp'}]
```

### Virtual dispatch: `dispatch_calls`

`dispatch_calls` (relation 18) is populated for virtual calls, and is
described as *conservative* - it may include targets that are not actually
reachable at runtime for the concrete receiver type, rather than
under-approximating. Verified against a call through a reference to a
polymorphic base (`box.flush()` inside `app::dispatch_probe`, where `box`
is `Box<int, 7>&` overriding `Base::flush`):

```python
q = start(symbol("app::dispatch_probe")) | out("dispatch_calls") | select(("qualified_name",))
```

```text
[{'qualified_name': 'app::Box::flush'}]
```

There is no devirtualization mode available from stored facts -
`out(relation, mode="devirtualized")` always fails validation with
`E_CAPABILITY: devirtualized traversal is unavailable; use dispatch_calls`.
Query `dispatch_calls` directly instead.

### Edge and site rows

Edge rows preserve source, destination, kind, position, access,
virtual-base, implicit, lexical, and count fields. Site rows preserve the
complete edge key, file, line, column, offset, receiver type, and
certainty. `certainty` is exposed as the **raw stored integer** (or `None`)
- the SDK does not decode it into named `exact`/`possible` labels the way
some native-side documentation describes the underlying concept, so treat
it as an opaque native evidence marker unless you have independently
confirmed its integer mapping:

```python
q = start(codebase()) | view("edge") | nodes() | sites()
for row in cb.executor.run(q.plan).to_dict()["nodes"]:
    print(row["kind"], row.get("certainty"), row["line"], row["col"])
```

```text
overrides None 11 8
calls None 10 21
calls None 13 10
calls None 13 19
uses None 13 32
calls 2 17 7
uses None 18 14
dispatch_calls 2 17 7
calls None 22 29
...
```

See [05-relations-and-graph-queries.md](05-relations-and-graph-queries.md)
for `sites()` used inside a `path()` witness, where every stored-relation
step carries its matching source sites.

Continue to
[05-relations-and-graph-queries.md](05-relations-and-graph-queries.md) for
typed graph navigation built on top of these views and relations.
