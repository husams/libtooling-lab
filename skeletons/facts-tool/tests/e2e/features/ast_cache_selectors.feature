@ast_cache
Feature: Imported ASTs retain their identity across source selector spellings
  Import prepares each translation unit with its compilation directory.
  Later commands resolve selectors from their invocation directory and reuse
  those ASTs and their shared transitive include graph without rebuilding them.

  Background:
    Given an isolated AST cache project
    And AST caching is enabled
    And import prepares two translation units built outside the invocation directory
    And uncommitted compile errors guard the imported inputs against reparsing

  Scenario Outline: Repeated consumers reuse import entries through equivalent selectors
    When the "<family>" consumer runs twice with "<selector>" source selection
    Then both selector invocations reuse the imported cache without rebuilding
    And the imported shared and transitive header metadata remains unchanged
    And the cached "<family>" result is complete

    Examples:
      | family        | selector           |
      | extract       | relative file      |
      | extract       | absolute file      |
      | extract       | nested invocation  |
      | extract       | dot relative file  |
      | extract       | duplicate absolute |
      | extract       | overlapping files  |
      | extract       | all sources        |
      | match         | relative file      |
      | match         | absolute file      |
      | match         | nested invocation  |
      | match         | dot relative file  |
      | match         | duplicate absolute |
      | match         | overlapping files  |
      | match         | all sources        |
      | dependency    | relative file      |
      | dependency    | absolute file      |
      | dependency    | nested invocation  |
      | dependency    | dot relative file  |
      | dependency    | duplicate absolute |
      | dependency    | overlapping files  |
      | dependency    | multiple absolute  |
      | variable-flow | relative file      |
      | variable-flow | absolute file      |
      | variable-flow | nested invocation  |
      | variable-flow | dot relative file  |
      | variable-flow | duplicate absolute |
      | variable-flow | overlapping files  |
      | variable-flow | all sources        |

  Scenario Outline: The reparse guards detect consumers running without caching
    When the guarded "<family>" consumer runs with caching disabled
    Then the uncached consumer reports the input reparse guard

    Examples:
      | family        |
      | extract       |
      | match         |
      | dependency    |
      | variable-flow |
