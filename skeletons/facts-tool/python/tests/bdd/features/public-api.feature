Feature: Complete public API contract
  Background:
    Given a valid paired facts and project database

  Scenario: Execute every semantic predicate helper
    When I execute every semantic helper and target-set constructor
    Then every helper produces a valid executable predicate

  Scenario: Resolve all source forms and precedence
    When I query codebase symbol and adapted entity sources
    Then USR qualified-name and spelling lookups agree

  Scenario: Paginate with honest truncation
    When I enumerate with a result cap and cursor
    Then truncation and the next page are explicit

  Scenario: Surface traversal budget exhaustion
    When I traverse with a one-state budget
    Then the result reports truncation

  Scenario: Surface witness reconstruction exhaustion
    When I reconstruct a path with a one-state budget
    Then the path result reports truncation

  Scenario: Apply all unknown policies and empty booleans
    When I evaluate unknown evidence and empty boolean sets
    Then exclude include error and empty-set semantics are explicit

  Scenario: Treat SQL-like values only as data
    When I query a SQL-like source and malformed catalog names
    Then stable source field and relation errors are returned
