Feature: Typed and fluent APIs
  Background:
    Given a valid paired facts and project database

  Scenario: Get typed objects
    When I get callable and record objects
    Then their persisted kinds and qualifiers are exposed

  Scenario: Navigate typed objects
    When I navigate callers callees fields and definitions
    Then navigation uses stored facts and project locations

  Scenario: Lower fluent chains
    When I build serializable and local fluent filters
    Then portable stages lower to a plan and callbacks remain local

  Scenario: Export complete result metadata
    When I export node row scalar path and empty results
    Then every shape retains completeness and provenance metadata
