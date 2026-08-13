Feature: C++ record inheritance
  Direct bases are stored with their order, access, and virtual inheritance state.

  Background:
    Given realistic shared-header declarations, definitions, parameters, and relations
    When the real facts-tool indexes both translation units using compile_commands.json

  Scenario Outline: Store direct inheritance from <source> to <destination>
    Then the direct inheritance relations include
      | source   | destination   | kind   | position   | flags   | count   |
      | <source> | <destination> | <kind> | <position> | <flags> | <count> |
    And exactly 4 direct inheritance relations are stored

    Examples:
      | source               | destination | kind | position | flags | count |
      | e2e::CompositeWidget | e2e::Widget | 2    | 0        | 0     | 1     |
      | e2e::CompositeWidget | e2e::Policy | 2    | 1        | 5     | 1     |
      | e2e::PrivateWidget   | e2e::Widget | 2    | 0        | 2     | 1     |
      | e2e::PublicWidget    | e2e::Widget | 2    | 0        | 0     | 1     |
