Feature: C++ function parameter defaults
  Written default expressions are retained, with typed evaluated values when
  Clang can fold them.

  Background:
    Given realistic shared-header declarations, definitions, parameters, and relations
    When the real facts-tool indexes both translation units using compile_commands.json

  Scenario: Default expressions and scalar values are queryable
    Then the persisted parameter defaults include
      | qualified_name       | position | name    | expression | evaluated_kind | evaluated_value |
      | e2e::defaultArguments | 0        | count   | 2 + 3      | integer        | 5               |
      | e2e::defaultArguments | 1        | enabled | true       | boolean        | true            |
      | e2e::defaultArguments | 2        | label   | "ready"    | string         | ready           |
      | e2e::defaultArguments | 3        | widget  | Widget{7}  | none           |                 |
