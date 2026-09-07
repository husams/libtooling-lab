@s021
Feature: Recover only missing graph evidence across components
  Background:
    Given an S-021 app calls an unextracted registered library

  Scenario: Recovery is opt in and ordinary traversal preserves stored evidence
    Then omitting S-021 recovery preserves the partial graph and stores

  Scenario: An empty match index still discovers a different registered component
    Given S-021 body generation is recorded by the fixture
    When S-021 missing recovery is requested
    Then S-021 recovers the library body with its stored command
    And S-021 generates the library once and never regenerates the app

  Scenario: Symbol matching does not substitute for body and call evidence
    Given the S-021 library has only symbol-match evidence
    When S-021 missing recovery is requested
    Then S-021 recovers the library body with its stored command

  Scenario: Valid body and call facts are reused without re-extraction
    Given the S-021 library already has valid call facts
    And S-021 body generation is recorded by the fixture
    When S-021 missing recovery is requested
    Then S-021 reuses existing facts without extraction
    And S-021 produces no new body generation

  Scenario: A failed library preserves the partial graph and terminates
    Given the S-021 registered library has a syntax error
    And S-021 persistent schemas are recorded
    When S-021 missing recovery is requested
    Then S-021 reports the failed library once with the partial graph
    And S-021 adds no persisted failure or attempt schema

  Scenario: Changed source and a new invocation permit recovery after failure
    Given the S-021 registered library has a syntax error
    When S-021 missing recovery is requested
    Then S-021 reports the failed library once with the partial graph
    When the S-021 library is repaired and recovery is requested again
    Then S-021 recovers the library body with its stored command

  Scenario: Recovery extraction never writes the match-only index
    Given the S-021 library has only symbol-match evidence
    And S-021 match-index writes are guarded
    When S-021 missing recovery is requested
    Then S-021 recovers the library body with its stored command
    And S-021 match-index contents remain unchanged

  Scenario: A complete discovery pass retains unavailable definition evidence
    Given the S-021 library contains no requested definition
    When S-021 missing recovery is requested
    Then S-021 keeps an honest unavailable definition boundary

  Scenario: Multiple wanted definitions share one library extraction
    Given the S-021 app needs two definitions in the same library TU
    And S-021 body generation is recorded by the fixture
    When S-021 missing recovery is requested
    Then S-021 recovers the library body with its stored command
    And S-021 generates the library once and never regenerates the app

  Scenario: Missing compiler input is a recovery gap rather than an unchanged digest
    Given the S-021 registered library input is missing
    And S-021 persistent schemas are recorded
    When S-021 missing recovery is requested
    Then S-021 reports the failed library once with the partial graph
    And S-021 adds no persisted failure or attempt schema

  Scenario: Other components do not exclude a definition in the app component
    Given the S-021 missing definition is in another TU of the app component
    And S-021 body generation is recorded by the fixture
    When S-021 missing recovery is requested
    Then S-021 finds the same-component definition without extracting unrelated TUs

  Scenario: Recovery is restricted to evidence reachable from the requested root
    Given the S-021 library already has valid call facts
    And S-021 also stores an unrelated root with a missing definition
    And S-021 body generation is recorded by the fixture
    When S-021 missing recovery is requested
    Then S-021 reuses existing facts without extraction
    And S-021 produces no new body generation

  Scenario: Newly discovered missing evidence is recovered in the same invocation
    Given recovering S-021 bridge reveals a missing definition in another TU
    When S-021 missing recovery is requested
    Then S-021 follows the newly discovered boundary until stored evidence is usable

  Scenario: Requested recovery failures also fail a callers query
    Given the S-021 registered library has a syntax error
    When S-021 recovery is requested with a callers query
    Then S-021 reports the failed library once with the partial graph

  Scenario: Requested recovery failures also fail a path query
    Given the S-021 registered library has a syntax error
    When S-021 recovery is requested with a path query
    Then S-021 reports the failed library once with the partial graph

  Scenario: Header matches use the registered including translation unit
    Given the S-021 match index locates bridge in a registered library header
    When S-021 missing recovery is requested
    Then S-021 recovers the library body with its stored command
