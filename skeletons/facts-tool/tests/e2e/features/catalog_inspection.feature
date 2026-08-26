Feature: Inspect the persisted project catalog
  Commands use the real executable, imported SQLite rows, and independent readback.
  Checkout sources and the separate facts database must remain unchanged.

  Background:
    Given an imported catalog with two repositories and independent components

  Scenario: Import persists the management fixture's source commands
    Then the catalog contains the imported source commands and directories
    And the catalog database is consistent

  Scenario Outline: Inspect persisted repositories and components
    When I run the catalog command "<command>"
    Then the catalog command succeeds
    And the catalog output contains "<value>"
    And the entire catalog is unchanged

    Examples:
      | command             | value    |
      | repo list           | demo     |
      | repo show demo      | core     |
      | component list      | neighbor |
      | component show core | core     |
      | dir list -c core    | src/deep |
