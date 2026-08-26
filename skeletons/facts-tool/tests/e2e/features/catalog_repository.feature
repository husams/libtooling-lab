Feature: Manage repository clones and ownership
  Commands use the real executable, imported SQLite rows, and independent readback.
  Checkout sources and the separate facts database must remain unchanged.

  Background:
    Given an imported catalog with two repositories and independent components

  Scenario: Register another clone without changing the active checkout
    When I run the catalog command "repo add-clone demo {second-clone} --label second"
    Then the catalog command succeeds
    And the second clone is registered but the original clone is active
    And the logical component directory and file rows are unchanged
    And the catalog database is consistent

  Scenario Outline: Switch a clone and extract using the same logical file identities
    Given a registered second clone of the demo repository
    When I run the catalog command "repo switch demo <target>"
    Then the catalog command succeeds
    And the active clone is the second checkout
    And the logical component directory and file rows are unchanged
    When I extract a source from the second checkout using only the stored database
    Then extraction from the second checkout persists the expected symbol

    Examples:
      | target         |
      | second         |
      | {second-clone} |

  Scenario: Remove a repository while retaining usable detached components
    When I run the catalog command "repo rm demo"
    Then the catalog command succeeds
    And the demo repository and its clones are absent
    And the core component is detached without changing resolved file paths
    And the neighbor component and its files are unchanged
    And the catalog database is consistent

  Scenario: Remove a repository and its components explicitly
    When I run the catalog command "repo rm demo --delete-components"
    Then the catalog command succeeds
    And the demo repository and its clones are absent
    And the core component and its directories and files are absent
    And the neighbor component and its files are unchanged
    And the catalog database is consistent

  Scenario: Registering the same clone twice preserves its identity
    Given a registered second clone of the demo repository
    When I run the catalog command "repo add-clone demo {second-clone} --label second"
    Then the catalog command succeeds
    And only one second clone is registered
    And the entire catalog is unchanged

  Scenario: Switching to an incomplete checkout preserves the active clone
    Given a registered second clone of the demo repository
    And the second clone is missing an imported source
    When I run the catalog command "repo switch demo second"
    Then the catalog command fails with "missing registered file"
    And the entire catalog is unchanged
