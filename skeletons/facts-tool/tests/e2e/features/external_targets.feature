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

  Scenario: Persist an implicit aligned runtime deallocation callee
    Given a B-027 compile database with one runtime-callee source
    When B-027 extraction runs for the runtime-callee source
    Then the B-027 extraction commits without invalid-USR diagnostics
    And the aligned runtime delete is one lightweight external call target

  Scenario: Persist the runtime callee reached through stream temporaries
    Given a B-027 compile database with the stream-temporary source
    When B-027 extraction runs for the stream-temporary source
    Then the B-027 extraction commits without invalid-USR diagnostics
    And the aligned runtime delete is one lightweight external call target

  Scenario: Reuse the implicit runtime callee across translation units
    Given a B-027 compile database with both runtime-callee sources
    When B-027 extraction runs for both runtime-callee sources
    Then the B-027 extraction commits without invalid-USR diagnostics
    And the aligned runtime delete is one lightweight external call target

  Scenario: Report a genuine external-symbol persistence failure
    Given a B-027 compile database with one runtime-callee source
    When B-027 runtime-target persistence is forced to fail on a rerun
    Then the relation-target failure is reported and the rerun is rolled back
