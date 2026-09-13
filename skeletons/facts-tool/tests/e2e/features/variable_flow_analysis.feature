@variable_flow
Feature: Variable flow analysis
  Variable-flow analysis persists an immutable graph of reaching reads,
  writes, updates, calls, and explicit unsupported boundaries.

  Background:
    Given the isolated variable-flow fixture project
    And the variable-flow fixture is extracted

  Scenario: Local reaching definitions include branches and loops
    When variable flow is analysed for function "variable_flow::root" and variable "result"
    Then the variable-flow run is complete
    And the variable-flow graph contains local reads, writes, and updates
    And the graph has reaching writes from both branch and loop statements

  Scenario: Stored compile commands drive a cross-TU analysis
    When variable flow is analysed for function "variable_flow::root" and variable "result" using stored compile commands
    Then the variable-flow run is complete
    And the graph includes cross-translation-unit producer and callee relationships
    And reference updates reach the caller while value copies stay separate

  Scenario: Explicit sources select the same paired project
    When variable flow is analysed for function "variable_flow::root" and variable "result" with sources
    Then the variable-flow run is complete
    And the graph includes cross-translation-unit producer and callee relationships

  Scenario: Unrelated work is excluded from the selected dependency
    When variable flow is analysed for function "variable_flow::root" and variable "result"
    Then the variable-flow run is complete
    And the unrelated call and variable are excluded from the dependency graph

  Scenario: Depth zero is intraprocedural
    When variable flow is analysed for function "variable_flow::root" and variable "result" with maximum depth 0
    Then the variable-flow run is complete
    And depth zero contains only the selected function
    And the graph contains no nodes deeper than 0

  Scenario: Bounded depth stops at the requested call boundary
    When variable flow is analysed for function "variable_flow::root" and variable "result" with maximum depth 1
    Then the variable-flow run is complete
    And the graph contains no nodes deeper than 1

  Scenario: Unlimited depth reaches a producer definition
    When variable flow is analysed for function "variable_flow::root" and variable "result"
    Then the variable-flow run is complete
    And the unlimited graph reaches the producer definition

  Scenario: Separate same-callee callsites retain separate return links
    When variable flow is analysed for function "variable_flow::repeated_helper" and variable "result"
    Then the variable-flow run is complete
    And both helper callsites have linked return nodes

  Scenario: Unresolved external calls are explicit boundaries
    When variable flow is analysed for function "variable_flow::external_boundary" and variable "seed"
    Then the variable-flow run is complete
    And the run records an explicit external boundary

  Scenario: Indirect calls are explicit boundaries
    When variable flow is analysed for function "variable_flow::indirect_boundary" and variable "seed"
    Then the variable-flow run has status "partial"
    And the run records an explicit indirect boundary

  Scenario: Shadowed variables can be selected by declaration line
    When variable flow is analysed for function "variable_flow::shadowed" and variable "value" at line 28
    Then the variable-flow run is complete
    And the graph contains no nodes deeper than 0

  Scenario: Ambiguous variable selection is rejected without publication
    When variable flow is analysed with invalid function "variable_flow::shadowed" and variable "value"
    Then the invalid variable-flow request publishes no run

  Scenario: Recursive analysis terminates with finite depth
    When variable flow is analysed for function "variable_flow::recursive" and variable "current"
    Then the variable-flow run is complete
    And the graph contains no nodes deeper than 1

  Scenario: Multiple runs append to one flow database
    When variable flow is analysed for function "variable_flow::root" and variable "result" repeatedly
    Then both variable-flow runs are readable and ordered

  Scenario: Parse failures publish no partial flow
    When variable flow is analysed with a source parse error
    Then the parse error publishes no variable-flow run

  Scenario: Default output leaves the input facts database untouched
    When the input facts database is snapshotted and variable flow uses its default output
    Then the input facts database is unchanged and the default flow database is readable

  Scenario: An alien output path is rejected
    When variable flow is asked to use the input facts database as output
    Then the alien output is rejected without changing the input facts database

  Scenario: Explicit output works without a YAML selector
    When variable flow is analysed with an explicit output and no YAML config
    Then the variable-flow run is complete
    And the explicit flow output is readable

  Scenario: Explicit output supports per-source facts templates
    When variable flow is analysed with a per-source facts template and explicit output
    Then the variable-flow run is complete
    And the per-source facts inputs are unchanged
    And the graph includes cross-translation-unit producer and callee relationships
