Feature: Call graph time limits and cancellation
  Operational interruption preserves an honest coherent partial result.

  Scenario: Time budgets report coherent partial results
    Given a large generated call graph is extracted
    Then the monotonic time budget reports a coherent partial result

  Scenario: Cancellation reports a coherent partial result
    Given a large generated call graph is extracted
    Then SIGINT reports a coherent cancelled result and exit 130
