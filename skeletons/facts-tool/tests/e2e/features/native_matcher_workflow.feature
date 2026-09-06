Feature: Native matcher workflow
  The native matcher keeps project metadata and extracted facts in explicit
  paired databases while preserving source selector and binding contracts.

  Scenario: Match uses separate project and facts databases
    Given a separately stored native matcher fixture
    When a symbol matcher runs with the explicit database pair
    Then the paired native match succeeds
    And the native call graph can traverse the matched facts

  Scenario: Invalid binding fails before writing through the paired workflow
    Given a separately stored native matcher fixture
    When an invalid symbol binding runs with the explicit database pair
    Then the paired native match fails with an actionable binding contract

  Scenario: Relative import selectors use the invocation directory
    Given a separate-build native matcher fixture
    When import runs with a relative source selector
    Then the relative native import succeeds
