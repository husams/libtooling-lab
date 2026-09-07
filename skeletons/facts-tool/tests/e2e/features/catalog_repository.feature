Feature: Manage repository clones and ownership
  Commands use the real executable, imported SQLite rows, and independent readback.
  Checkout sources and graph facts remain unchanged; writes may invalidate entries.

  Background:
    Given an imported catalog with two repositories and independent components

  Scenario: Register a new repository with its first checkout
    When I run the catalog command "repo add vendor {external-root} --label main --remote https://example.invalid/vendor.git"
    Then the catalog command succeeds
    And the vendor repository is registered with its checkout active
    And only the vendor repository and its clone were added
    And the catalog database is consistent
    When I run the catalog command "repo show vendor"
    Then the catalog command succeeds
    And the catalog output shows the vendor checkout as the active clone

  Scenario: A registered repository without a label or remote accepts components
    When I run the catalog command "repo add vendor {external-root}"
    Then the catalog command succeeds
    And the vendor repository has no label and no remote
    When I run the catalog command "component add --path {external-root} --name vendor-lib --repo vendor --no-git"
    Then the catalog command succeeds
    And the vendor-lib component belongs to the vendor repository
    And the original file rows are unchanged
    And the catalog database is consistent

  Scenario: Register a repository into a fresh configuration
    When I register the vendor repository against a new configuration
    Then the new configuration holds the vendor repository with its active clone and no files

  Scenario Outline: Rejected repository registrations leave the catalog unchanged
    When I run the catalog command "repo add <name> <path>"
    Then the catalog command fails with "<diagnostic>"
    And the entire catalog is unchanged
    And the catalog database is consistent

    Examples:
      | name   | path            | diagnostic                              |
      | demo   | {external-root} | repository 'demo' already registered    |
      | vendor | {missing-path}  | directory not found                     |
      | vendor | {checkout}      | clone path or label already registered  |

  Scenario: Registering a repository without a checkout path is rejected by the parser
    When I run the catalog command "repo add vendor"
    Then the catalog parser rejects the invalid arguments
    And the entire catalog is unchanged

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
