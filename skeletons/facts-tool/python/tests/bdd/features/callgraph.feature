Feature: Persisted callgraph run reader
  Scenario: Read a native schema14 run without replacing ordinary navigation
    Given a current native schema14 pair
    When I read its persisted graph and ordinary navigation
    Then graph provenance and relation navigation are distinct
