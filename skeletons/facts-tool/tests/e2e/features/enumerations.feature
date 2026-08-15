Feature: C++ enumerations and enumerators
  Enumerations retain their type and definition facts, while enumerators are
  first-class symbols with computed values and ownership.

  Background:
    Given realistic shared-header declarations, definitions, parameters, and relations
    When the real facts-tool indexes both translation units using compile_commands.json

  Scenario: Enum facts and computed enumerator values are queryable
    Then the persisted enumeration facts include
      | qualified_name | node | is_definition | is_scoped | has_fixed_underlying_type |
      | e2e::Mode      | 3    | 1             | 1         | 1                         |
    And the persisted enumerator facts include
      | qualified_name   | node | is_definition | value | initializer_expression |
      | e2e::Mode::Fast  | 6    | 1             | 0     |                        |
      | e2e::Mode::Slow  | 6    | 1             | 5     | 5                      |
      | e2e::Mode::Later | 6    | 1             | 6     |                        |
    And the persisted enum ownership relations include
      | source    | destination      | kind |
      | e2e::Mode | e2e::Mode::Fast  | 3    |
      | e2e::Mode | e2e::Mode::Slow  | 3    |
      | e2e::Mode | e2e::Mode::Later | 3    |
