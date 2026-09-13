Feature: Preserve facts when a callable target has an unpersisted USR
  A valid compiler runtime target must not roll back otherwise valid facts.

  Scenario: B-022 stream temporary calls commit with sibling facts
    Given a B-022 stream compile database
    When B-022 extraction indexes the fixture
    Then B-022 extraction commits without incomplete diagnostics
    And the B-022 canary and sibling Calls are persisted

  Scenario: B-022 builtin constructor targets commit across a forced rerun
    Given a B-022 builtin compile database
    When B-022 extraction indexes the fixture twice
    Then B-022 extraction commits without incomplete diagnostics
    And the B-022 builtin record and constructor call sites are canonical
