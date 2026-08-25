Feature: Filtered external relation targets
  System-header declarations used by project-owned relations are retained as
  lightweight symbols without indexing their header bodies.

  Scenario: Extract aliases and compound external parameter types successfully
    Given a reproducing compile database for filtered external targets
    When the real extraction command indexes the external-target fixture
    Then the external-target extraction exits successfully without incomplete diagnostics
    And RelationResult aliases a lightweight external std::expected symbol
    And compound external field types resolve to lightweight symbols
    And the compound external parameter types retain their modifiers

  Scenario: Extract a specialization with a filtered external primary successfully
    Given a reproducing compile database for a filtered external template primary
    When the real extraction command indexes the external-template-specialization fixture
    Then the external-target extraction exits successfully without incomplete diagnostics
    And the specialization points to a lightweight external std::hash primary

  Scenario: Preserve and extract a clang++ standard-library command
    Given a reproducing clang++ compile database for filtered external targets
    When the real extraction command indexes the external-target fixture
    Then the stored external-target driver is preserved
    And the external-target extraction exits successfully without incomplete diagnostics

  Scenario: Preserve and extract a GNU g++ libstdc++ command
    Given a reproducing GNU g++ compile database for filtered external targets
    When the real extraction command indexes the external-target fixture
    Then the stored external-target driver is preserved
    And the external-target extraction exits successfully without incomplete diagnostics
