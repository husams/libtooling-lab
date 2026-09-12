Feature: Persisted callgraph run reader
  Scenario: Read a native schema13 run without replacing ordinary navigation
    Given a current native schema13 pair
    When I read its persisted graph and ordinary navigation
    Then graph provenance and relation navigation are distinct
