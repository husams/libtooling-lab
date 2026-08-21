Feature: C++ aliases
  Typedef declarations, using aliases, and alias templates are ordinary symbols
  that point at their target symbols without a separate alias model.

  Background:
    Given realistic shared-header declarations, definitions, parameters, and relations
    When the real facts-tool indexes both translation units using compile_commands.json

  Scenario: Typedef and using declarations are symbols with alias relations
    Then the alias symbols include
      | qualified_name      | node |
      | e2e::WidgetTypedef  | 5    |
      | e2e::WidgetAlias    | 5    |
    And the alias relations include
      | source              | destination |
      | e2e::WidgetTypedef  | e2e::Widget |
      | e2e::WidgetAlias    | e2e::Widget |
    And the facts database excludes the legacy type alias table

  Scenario: Alias templates retain their non-type slots and target relation
    Then the alias symbols include
      | qualified_name   | node |
      | e2e::StructAlias | 5    |
    And the declared template arguments are
      | qualified_name   | position | name | is_parameter_pack | is_non_type | is_template_template |
      | e2e::StructAlias | 0        | N    | 0                 | 1           | 0                    |
    And the alias relations include
      | source           | destination         |
      | e2e::StructAlias | e2e::StructTemplate |
