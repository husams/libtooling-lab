# Coding Instructions

## Functional, Declarative Style

Use declarative functional composition by default. Code should describe the
sequence of transformations instead of manually coordinating mutable state,
branches, and intermediate results.

### Required

- Build extraction flows from small, composable functions.
- Keep each function focused on one transformation or validation.
- Compose fallible transformations with `std::expected`, `and_then`,
  `transform`, or the project pipeline operator (`|`).
- Use `std::optional` only when absence is a valid result, not as an error.
- Use `std::expected<T, Error>` when failure has a meaningful reason.
- Keep pipelines split across multiple lines, with one stage per line.
- Keep shared logic in one extractor and reuse it from every symbol
  specialization.
- Keep model types plain value objects. Extraction behavior belongs in
  extractors, not model constructors.
- Keep visitors as orchestration only: invoke an extractor and pass successful
  results to the store.
- Put each symbol specialization in its own small header and source file.
- Localize unavoidable `if` statements to small leaf validation functions.

Example:

```cpp
return extractLocation(sourceManager, node.getLocation())
       | extractIdentity(node)
       | extractNamespace(node)
       | toSymbol(node);
```

### Avoid

- Long imperative extraction functions.
- Repeated `if`/`else` chains in visitors or pipeline orchestration.
- Mutating a result across unrelated extraction stages.
- Duplicating normalization, validation, or conversion logic.
- Reimplementing common location, identity, namespace, state, or relation
  extraction inside individual symbol extractors.
- Adding wrapper functions that only rename or forward to another extractor.
- Introducing class hierarchies, type erasure, or framework abstractions for a
  pipeline that can be expressed with functions and value types.

Prefer the smallest composable primitive that removes real duplication. Do not
create abstractions solely for possible future use.
