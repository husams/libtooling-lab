Feature: Function parameter types
  Parameters refer either to captured user-defined symbols or predefined primitive symbols.

  Background:
    Given realistic shared-header declarations, definitions, parameters, and relations
    When the real facts-tool indexes both translation units using compile_commands.json

  Scenario Outline: Resolve user-defined parameter <position> <name> to <symbol>
    Then the parameters for e2e::userDefinedTypes include
      | position   | name   | symbol   |
      | <position> | <name> | <symbol> |
    And e2e::userDefinedTypes has exactly 8 parameters

    Examples:
      | position | name      | symbol       |
      | 0        | value     | e2e::Widget  |
      | 1        | pointer   | e2e::Widget  |
      | 2        | reference | e2e::Widget  |
      | 3        | values    | e2e::Widget  |
      | 4        | mode      | e2e::Mode    |
      | 5        | count     | e2e::Count   |
      | 6        | payload   | e2e::Payload |
      | 7        | policy    | e2e::Policy  |

  Scenario: Primitive parameter forms resolve to predefined symbols
    Then the primitive parameters for e2e::primitiveTypes are
      | position | name        |
      | 0        | signedValue |
      | 1        | enabled     |
      | 2        | ratio       |
      | 3        | text        |
      | 4        | payload     |
    And their type IDs are positive predefined FileId-0 SymbolIds
    And their type IDs are all distinct
