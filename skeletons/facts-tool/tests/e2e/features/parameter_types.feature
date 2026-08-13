Feature: Function parameter types
  Parameters refer either to captured user-defined symbols or predefined primitive symbols.

  Background:
    Given realistic shared-header declarations, definitions, parameters, and relations
    When the real facts-tool indexes both translation units using compile_commands.json

  Scenario Outline: Resolve user-defined parameter <position> <name> to <symbol>
    Then the parameters for e2e::userDefinedTypes include
      | position   | name   | symbol   | is_pointer   | is_lvalue_reference   | is_rvalue_reference   | is_forwarding_reference   | is_const   | is_pack   |
      | <position> | <name> | <symbol> | <is_pointer> | <is_lvalue_reference> | <is_rvalue_reference> | <is_forwarding_reference> | <is_const> | <is_pack> |
    And e2e::userDefinedTypes has exactly 8 parameters

    Examples:
      | position | name      | symbol       | is_pointer | is_lvalue_reference | is_rvalue_reference | is_forwarding_reference | is_const | is_pack |
      | 0        | value     | e2e::Widget  | no         | no                  | no                  | no                      | no       | no      |
      | 1        | pointer   | e2e::Widget  | yes        | no                  | no                  | no                      | no       | no      |
      | 2        | reference | e2e::Widget  | no         | yes                 | no                  | no                      | no       | no      |
      | 3        | values    | e2e::Widget  | yes        | no                  | no                  | no                      | no       | no      |
      | 4        | mode      | e2e::Mode    | no         | no                  | no                  | no                      | no       | no      |
      | 5        | count     | e2e::Count   | no         | no                  | no                  | no                      | no       | no      |
      | 6        | payload   | e2e::Payload | no         | no                  | no                  | no                      | no       | no      |
      | 7        | policy    | e2e::Policy  | no         | no                  | no                  | no                      | no       | no      |

  Scenario: Primitive parameter forms resolve to predefined symbols
    Then the primitive parameters for e2e::primitiveTypes are
      | position | name        | is_pointer | is_lvalue_reference | is_rvalue_reference | is_forwarding_reference | is_const | is_pack |
      | 0        | signedValue | no         | no                  | no                  | no                      | no       | no      |
      | 1        | enabled     | no         | no                  | no                  | no                      | no       | no      |
      | 2        | ratio       | no         | no                  | no                  | no                      | no       | no      |
      | 3        | text        | yes        | no                  | no                  | no                      | yes      | no      |
      | 4        | payload     | yes        | no                  | no                  | no                      | no       | no      |
    And their type IDs are positive predefined FileId-0 SymbolIds
    And their type IDs are all distinct
