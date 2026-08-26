Feature: Reject catalog errors without partial changes
  Commands use the real executable, imported SQLite rows, and independent readback.
  Checkout sources and the separate facts database must remain unchanged.

  Background:
    Given an imported catalog with two repositories and independent components

  Scenario Outline: Reject unknown objects without changing the database
    When I run the catalog command "<command>"
    Then the catalog command rejects the unknown object
    And the entire catalog is unchanged

    Examples:
      | command                          |
      | repo show missing-repository     |
      | repo switch demo missing-clone   |
      | component show missing-component |
      | dir rm --id 999999               |

  Scenario: A failed cascading deletion rolls back every catalog change
    Given SQLite rejects deletion of the core source files
    When I run the catalog command "repo rm demo --delete-components"
    Then the catalog command reports the SQLite deletion failure
    And the entire catalog is unchanged
    And the catalog database is consistent

  Scenario Outline: A late repository failure rolls back detachments and cascades
    Given SQLite rejects the final repository deletion
    When I run the catalog command "repo rm demo <option>"
    Then the catalog command fails with "forced repository deletion failure"
    And the entire catalog is unchanged
    And the catalog database is consistent

    Examples:
      | option              |
      |                     |
      | --delete-components |
