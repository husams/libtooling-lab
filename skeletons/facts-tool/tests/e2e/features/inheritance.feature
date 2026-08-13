Feature: C++ record inheritance
  Direct bases are stored with their order, access, and virtual inheritance state.

  Scenario: Direct inheritance produces typed symbol relations
    Given realistic shared-header declarations, definitions, parameters, and relations
    When the real facts-tool indexes both translation units using compile_commands.json
    Then the direct inheritance relations are
      | source               | destination | kind | position | flags | count |
      | e2e::CompositeWidget | e2e::Widget | 2    | 0        | 0     | 1     |
      | e2e::CompositeWidget | e2e::Policy | 2    | 1        | 5     | 1     |
      | e2e::PrivateWidget   | e2e::Widget | 2    | 0        | 2     | 1     |
      | e2e::PublicWidget    | e2e::Widget | 2    | 0        | 0     | 1     |
