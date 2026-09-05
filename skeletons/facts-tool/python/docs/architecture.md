# Architecture

The package uses one-way dependencies to keep every hand-authored file at most
100 physical lines:

```text
queryplan types/builders -> validation/catalogs
database/schema/pairing -> view loaders -> neighbors/predicates
source/stage executors -> Executor -> Result
Executor -> GraphQuery -> typed entities and fluent facade
```

`database` owns safe opening, `schema` owns role/version validation, `pairing`
owns cross-database FileId checks, and `paths` owns project registry resolution.
Individual view loaders adapt rows to domain-scoped dictionaries.

Frozen queryplan dataclasses contain no database handles. Builders create
sources, predicates, and stages; validation checks allowlisted grammar and
shape before execution. Canonical serialization walks only frozen data.

The executor resolves a source, applies stages through small operation modules,
and converts state to a provenance-bearing result. Traversal and predicate
quantifiers share the same neighbor adapter. Set operands and path targets
execute as independent immutable plans against the same read-only pair.

Typed and fluent APIs are facades over `Executor`; they do not issue alternate
SQL. Only fixed adapter statements contain identifiers, selected from internal
catalogs. User strings are SQLite parameters or in-memory comparison values.

Runtime dependencies are Python's standard library. Tests use real temporary
SQLite files, pytest, and pytest-bdd without mocking database behavior.
`scripts/check_import_boundaries.py` rejects top-level runtime import cycles
and imports every package module as an executable dependency-boundary gate.
