Feature: Manage components and stored compilation commands
  Commands use the real executable, imported SQLite rows, and independent readback.
  Checkout sources and the separate facts database must remain unchanged.

  Background:
    Given an imported catalog with two repositories and independent components

  Scenario: Register a component without extracting or importing sources
    When I run the catalog command "component add --path {external-root} --name vendor --kind external --no-git"
    Then the catalog command succeeds
    And the external component is persisted at its requested root
    And the original file rows are unchanged
    And the catalog database is consistent

  Scenario Outline: Change or clear a component version
    Given the core component has version "v1"
    When I run the catalog command "component set-version core <version>"
    Then the catalog command succeeds
    And the stored core version is "<expected>"
    And the original file rows are unchanged

    Examples:
      | version | expected |
      | v2      | v2       |
      |         |          |

  Scenario: Export persisted compilation commands as usable JSON
    When I run the catalog command "component compile-commands core"
    Then the catalog command succeeds
    And the exported commands match the stored core source commands
    And the entire catalog is unchanged

  Scenario Outline: Preview and remove one component by an exact selector
    When I run the catalog command "component rm <selector> --dry-run"
    Then the catalog command succeeds
    And the entire catalog is unchanged
    When I run the catalog command "component rm <selector>"
    Then the catalog command succeeds
    And the core component and its directories and files are absent
    And the neighbor component and its files are unchanged
    And the catalog database is consistent

    Examples:
      | selector          |
      | --name core       |
      | --id {core-id}    |
      | --path {core-root} |

  Scenario: Add a component inside an existing repository
    Given an unregistered directory inside the active checkout
    When I run the catalog command "component add --path {new-component} --name extension --repo demo --no-git"
    Then the catalog command succeeds
    And the new component is relative to the demo checkout
    And the original file rows are unchanged
    And the catalog database is consistent

  Scenario: A registered component without imported files exports an empty array
    When I run the catalog command "component add --path {external-root} --name vendor --kind external"
    Then the catalog command succeeds
    When I run the catalog command "component compile-commands vendor"
    Then the catalog command succeeds
    And the exported command list is empty
