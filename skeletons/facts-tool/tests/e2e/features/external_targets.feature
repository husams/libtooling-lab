Feature: Filtered external alias and compound type targets
  System-header declarations used by aliases and compound parameter types are
  retained as lightweight symbols without indexing their header bodies.

  Scenario: Extract aliases and compound external parameter types successfully
    Given a reproducing compile database for filtered external targets
    When the real extract subcommand indexes the external-target fixture
    Then the external-target extraction exits successfully without incomplete diagnostics
    And RelationResult aliases a lightweight external std::expected symbol
    And compound external field types resolve to lightweight symbols
    And the compound external parameter types retain their modifiers
