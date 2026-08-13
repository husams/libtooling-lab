Feature: C++ record inheritance
  Direct bases are stored with their order, access, and virtual inheritance state.

  Background:
    Given realistic shared-header declarations, definitions, parameters, and relations
    When the real facts-tool indexes both translation units using compile_commands.json

  Scenario Outline: Store direct inheritance from <source> to <destination>
    Then the direct inheritance relations include
      | source   | destination   | kind   | position   | access   | is_virtual_base   | is_implicit   | is_lexical   | count   |
      | <source> | <destination> | <kind> | <position> | <access> | <is_virtual_base> | <is_implicit> | <is_lexical> | <count> |
    And exactly 4 direct inheritance relations are stored

    Examples:
      | source               | destination | kind | position | access    | is_virtual_base | is_implicit | is_lexical | count |
      | e2e::CompositeWidget | e2e::Widget | 2    | 0        | public    | no              | no          | no         | 1     |
      | e2e::CompositeWidget | e2e::Policy | 2    | 1        | protected | yes             | no          | no         | 1     |
      | e2e::PrivateWidget   | e2e::Widget | 2    | 0        | private   | no              | no          | no         | 1     |
      | e2e::PublicWidget    | e2e::Widget | 2    | 0        | public    | no              | no          | no         | 1     |
