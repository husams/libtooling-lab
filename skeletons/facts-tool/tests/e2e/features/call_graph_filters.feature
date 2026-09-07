Feature: Optional call graph filters and traversal budgets
  Controls are request-local and never narrow default cross-component traversal.

  Background:
    Given a three-component cyclic call graph is extracted

  Scenario: Default traversal crosses every registered component without a hidden cap
    Then unfiltered traversal crosses all components without implicit limits

  Scenario: Component and project-library filters apply only when requested
    Then explicit component and calls-scope filters expose their boundaries

  Scenario: Unknown component selectors identify available candidates
    Then unknown graph components fail with candidates

  Scenario: Structural budgets report exact partial frontiers
    Then each explicit structural budget reports its exact frontier

  Scenario: All-root traversal reports every root skipped by a budget
    Then a bounded all-root traversal preserves every skipped root

  Scenario: Exact-depth leaves and cycles terminate honestly
    Then an exact-depth leaf is complete and cycles terminate without a cap

  Scenario: Invalid budgets are usage errors and requests persist no state
    Then invalid graph budgets fail as usage errors and requests leave no cache
    And operational JSON failures report error truncation and exit one
