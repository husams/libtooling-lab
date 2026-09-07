@s021
Feature: Recovery discovery does not mistake candidates for complete evidence
  Background:
    Given an S-021 app calls an unextracted registered library

  Scenario: A declaration-only index hit does not exclude the real definition
    Given S-021 has indexed only the bridge declaration in the app
    When S-021 missing recovery is requested
    Then S-021 recovers the library body with its stored command

  Scenario: A shared inline definition is extracted through only one candidate TU
    Given S-021 bridge is defined in a header included by two registered TUs
    And S-021 body generation is recorded by the fixture
    When S-021 missing recovery is requested
    Then S-021 generates the library once and never regenerates the app
