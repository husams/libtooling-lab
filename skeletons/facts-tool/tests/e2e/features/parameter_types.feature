Feature: Function parameter types
  Parameters refer either to captured user-defined symbols or predefined primitive symbols.

  Background:
    Given realistic shared-header declarations, definitions, parameters, and relations
    When the real facts-tool indexes both translation units using compile_commands.json

  Scenario: User-defined parameter forms resolve to captured symbols
    Then user-defined parameter forms resolve to their captured SymbolIds

  Scenario: Primitive parameter forms resolve to predefined symbols
    Then primitive parameter forms use distinct predefined FileId-0 SymbolIds
