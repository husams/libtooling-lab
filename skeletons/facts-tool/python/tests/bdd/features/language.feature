Feature: Declarative query language
  Background:
    Given a valid paired facts and project database

  Scenario: Enumerate and filter symbols
    When I select defined function names
    Then the names are "run,save"

  Scenario: Compose boolean comparisons
    When I filter with every comparison and boolean constructor
    Then the names are "run,save"

  Scenario: Evaluate relationship quantifiers
    When I evaluate every relationship quantifier
    Then all quantifier expectations hold

  Scenario: Reuse a bounded traversal prefix
    When I traverse calls from run through depth two
    Then the names are "save,persist"

  Scenario: Deduplicate diamonds and bounded cycles
    When I traverse diamond and cyclic call graphs
    Then each reachable identity appears once

  Scenario: Apply all set operations
    When I apply union intersection and except
    Then all set operation expectations hold

  Scenario: Shape deterministic results
    When I select distinct ordered limited names and count
    Then all shaping expectations hold

  Scenario: Produce a ranked evidence path
    When I find a ranked call path from run to persist
    Then the path has two steps and source evidence

  Scenario: Navigate direct reverse type use
    When I find direct users of int
    Then type-use witnesses are returned honestly
