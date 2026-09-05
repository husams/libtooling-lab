Feature: Persisted typed facts
  Background:
    Given a valid paired facts and project database

  Scenario: Query callable return and qualifiers
    When I query callable qualifiers and return facts
    Then canonical spelling and return target remain distinct

  Scenario: Query enumerations and initializers
    When I query enum enumerator and initializer views
    Then written evaluated and underlying values are preserved

  Scenario: Preserve template slots and supplied values
    When I query every declared slot and supplied template value
    Then order pack kind and flags use conventional names
