Feature: Stable extraction across reruns
  Repeated and concurrent extraction does not change file or symbol identity.

  Background:
    Given realistic shared-header declarations, definitions, parameters, and relations
    When the real facts-tool indexes both translation units using compile_commands.json

  Scenario: Serial and concurrent reruns preserve identifiers
    When indexing is repeated once
    Then FileIds and SymbolIds match the initial extraction
    When two extractor processes run concurrently
    Then FileIds and SymbolIds still match the initial extraction
    And no duplicate SymbolIds are stored
