@s021
Feature: Partial call matches are not complete body evidence
  Scenario: A stored call does not hide a missing call in the same function
    Given an S-021 app calls an unextracted registered library
    And S-021 has matched only one of two calls in the library body
    When S-021 missing recovery is requested
    Then S-021 recovers the missing second call despite the existing first call
