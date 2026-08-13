Feature: Function parameter types
  Parameters refer either to captured user-defined symbols or predefined primitive symbols.
  Integer boolean values are shown exactly as persisted by SQLite: 0 or 1.

  Background:
    Given realistic shared-header declarations, definitions, parameters, and relations
    When the real facts-tool indexes both translation units using compile_commands.json

  Scenario Outline: Resolve user-defined parameter <position> <name> to <symbol>
    Then the persisted parameters for e2e::userDefinedTypes include
      | position   | name   | type_qualified_name   | is_pointer   | is_lvalue_reference   | is_rvalue_reference   | is_forwarding_reference   | is_const   | is_pack   |
      | <position> | <name> | <symbol>              | <is_pointer> | <is_lvalue_reference> | <is_rvalue_reference> | <is_forwarding_reference> | <is_const> | <is_pack> |
    And e2e::userDefinedTypes has exactly 8 parameters

    Examples:
      | position | name      | symbol       | is_pointer | is_lvalue_reference | is_rvalue_reference | is_forwarding_reference | is_const | is_pack |
      | 0        | value     | e2e::Widget  | 0          | 0                   | 0                   | 0                       | 0        | 0       |
      | 1        | pointer   | e2e::Widget  | 1          | 0                   | 0                   | 0                       | 0        | 0       |
      | 2        | reference | e2e::Widget  | 0          | 1                   | 0                   | 0                       | 0        | 0       |
      | 3        | values    | e2e::Widget  | 1          | 0                   | 0                   | 0                       | 0        | 0       |
      | 4        | mode      | e2e::Mode    | 0          | 0                   | 0                   | 0                       | 0        | 0       |
      | 5        | count     | e2e::Count   | 0          | 0                   | 0                   | 0                       | 0        | 0       |
      | 6        | payload   | e2e::Payload | 0          | 0                   | 0                   | 0                       | 0        | 0       |
      | 7        | policy    | e2e::Policy  | 0          | 0                   | 0                   | 0                       | 0        | 0       |

  Scenario: Primitive parameter forms resolve to predefined symbols
    Then the persisted primitive parameter fields for e2e::primitiveTypes are
      | position | name        | is_pointer | is_lvalue_reference | is_rvalue_reference | is_forwarding_reference | is_const | is_pack |
      | 0        | signedValue | 0          | 0                   | 0                   | 0                       | 0        | 0       |
      | 1        | enabled     | 0          | 0                   | 0                   | 0                       | 0        | 0       |
      | 2        | ratio       | 0          | 0                   | 0                   | 0                       | 0        | 0       |
      | 3        | text        | 1          | 0                   | 0                   | 0                       | 1        | 0       |
      | 4        | payload     | 1          | 0                   | 0                   | 0                       | 0        | 0       |
    And their type IDs are positive predefined FileId-0 SymbolIds
    And their type IDs are all distinct
