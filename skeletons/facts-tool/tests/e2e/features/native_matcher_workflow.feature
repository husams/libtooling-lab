Feature: Native matcher workflow
  The native matcher keeps project metadata and extracted facts in explicit
  paired databases while preserving source selector and binding contracts.

  Scenario: Match uses separate project and facts databases
    Given a separately stored native matcher fixture
    When a symbol matcher runs with the explicit database pair
    Then the paired native match succeeds

  Scenario: Native matching writes callgraph facts without extraction
    Given a separately stored native matcher fixture
    When a direct-call matcher runs twice with the explicit database pair
    Then the native call graph can traverse the matched facts twice

  Scenario: Opt-in expression matching persists field effects and source evidence
    Given a separately stored expression evidence fixture
    When an expression matcher captures field evidence
    Then the expression evidence rows record direct field effects and a source fingerprint
    When the expression source changes and matching runs again
    Then both source fingerprints remain queryable
    When a symbol matcher captures source regions
    Then the source region rows retain definition ranges and freshness
    When a symbol matcher captures unsupported source regions
    Then unavailable source region reasons remain explicit

  Scenario: Expression evidence keeps distinct translation-unit provenance
    Given a two-translation-unit expression evidence fixture
    When an expression matcher captures both translation units
    Then each translation unit retains its own source fingerprint
    When a project pair omits one evidence translation unit
    Then pair validation rejects the evidence store

  Scenario: Unavailable expression identities remain translation-unit scoped
    Given two unavailable expression fixtures with equal source offsets
    When an expression matcher captures unavailable field evidence
    Then unavailable occurrences remain distinct with explicit reasons

  Scenario: Failed expression persistence does not publish partial evidence
    Given a separately stored expression evidence fixture
    When an expression matcher captures field evidence
    And the expression facts store becomes read-only for a second match
    Then the failed expression match leaves the prior evidence unchanged

  Scenario: Failed expression matching rolls back a prior translation unit
    Given a separately stored expression evidence fixture
    When a valid then invalid expression matcher runs
    Then the failed expression match leaves every facts table unchanged

  Scenario: Ordinary extraction leaves expression evidence opt-in
    Given a separately stored expression evidence fixture
    When ordinary extraction runs for the expression fixture
    Then ordinary extraction has no expression or source evidence rows

  Scenario: Invalid binding fails before writing through the paired workflow
    Given a separately stored native matcher fixture
    When an invalid symbol binding runs with the explicit database pair
    Then the paired native match fails with an actionable binding contract
    And the matcher help lists the supported binding contracts

  Scenario: Relative import selectors use the invocation directory
    Given a separate-build native matcher fixture
    When import runs with a relative source selector
    Then the relative native import succeeds

  Scenario: Relative and absolute import selectors select the same command
    Given a separate-build native matcher fixture
    When import runs with a relative source selector
    And import runs with an absolute source selector
    Then relative and absolute native imports select the same command

  Scenario: Invalid relative import selectors explain their base
    Given a separate-build native matcher fixture
    When import runs with an invalid relative source selector
    Then the invalid native selector reports its invocation base

  Scenario: Match defaults create the configured facts template
    Given a default-configured native matcher fixture
    When a native matcher runs with configured defaults
    Then the default native match succeeds and materializes facts_template
