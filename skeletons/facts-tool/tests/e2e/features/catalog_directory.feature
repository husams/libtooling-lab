Feature: Manage exact indexed directories
  Commands use the real executable, imported SQLite rows, and independent readback.
  Checkout sources and the separate facts database must remain unchanged.

  Background:
    Given an imported catalog with two repositories and independent components

  Scenario Outline: Preview and remove an indexed directory without deleting its siblings
    When I run the catalog command "dir rm <selector> --dry-run"
    Then the catalog command succeeds
    And the entire catalog is unchanged
    When I run the catalog command "dir rm <selector>"
    Then the catalog command succeeds
    And only the selected directory and its file rows are removed
    And the neighbor component and its files are unchanged
    And the catalog database is consistent

    Examples:
      | selector                      |
      | --id {deep-id}                 |
      | --path src/deep --component core |

  Scenario: A component scope disambiguates a shared directory path
    Given two components contain the same indexed directory path
    When I run the catalog command "dir rm --path src/deep --component core"
    Then the catalog command succeeds
    And only the selected directory and its file rows are removed
    And the neighbor component and its files are unchanged
    And the catalog database is consistent

  Scenario: An identifier from a different component cannot bypass the scope
    When I run the catalog command "dir rm --id {deep-id} --component neighbor"
    Then the catalog command rejects the unknown object
    And the entire catalog is unchanged
