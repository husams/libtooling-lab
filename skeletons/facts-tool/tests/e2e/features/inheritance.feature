Feature: C++ record inheritance
  Direct bases are stored with their order, access, and virtual inheritance state.
  Integer boolean values are shown exactly as persisted by SQLite: 0 or 1.

  Background:
    Given realistic shared-header declarations, definitions, parameters, and relations
    When the real facts-tool indexes both translation units using compile_commands.json

  Scenario Outline: Store direct inheritance from <source> to <destination>
    Then the persisted direct inheritance fields include
      | source   | destination   | kind   | position   | access   | is_virtual_base   | is_implicit   | is_lexical   | count   |
      | <source> | <destination> | <kind> | <position> | <access> | <is_virtual_base> | <is_implicit> | <is_lexical> | <count> |
    And exactly 4 direct inheritance relations are stored

    Examples:
      | source               | destination | kind | position | access    | is_virtual_base | is_implicit | is_lexical | count |
      | e2e::CompositeWidget | e2e::Widget | 2    | 0        | public    | 0               | 0           | 0          | 1     |
      | e2e::CompositeWidget | e2e::Policy | 2    | 1        | protected | 1               | 0           | 0          | 1     |
      | e2e::PrivateWidget   | e2e::Widget | 2    | 0        | private   | 0               | 0           | 0          | 1     |
      | e2e::PublicWidget    | e2e::Widget | 2    | 0        | public    | 0               | 0           | 0          | 1     |
