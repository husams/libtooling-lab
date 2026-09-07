Feature: Persisted callgraph run reader
  Scenario: Read a native schema12 run without replacing ordinary navigation
    Given a current native schema12 pair
    When I read its persisted graph and ordinary navigation
    Then graph provenance and relation navigation are distinct
