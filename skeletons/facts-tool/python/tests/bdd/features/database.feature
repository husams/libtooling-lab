Feature: Read-only database lifecycle and failures
  Scenario: Preserve both databases
    Given a valid paired facts and project database
    When successful invalid and error queries are attempted
    Then neither database nor adjacent files change

  Scenario: Reject a missing path without creating it
    Given missing database paths
    When I try to open the pair
    Then E_DATABASE is raised and no path is created

  Scenario: Reject the same physical file
    Given one SQLite database path for both roles
    When I try to open the pair
    Then E_DATABASE_ROLE is raised

  Scenario: Reject wrong roles and unsupported schemas
    Given reversed and incompatible databases
    When I try each invalid pair
    Then role and schema error codes are stable

  Scenario: Reject missing incomplete and mismatched registries
    Given project databases with invalid registry mappings
    When I try each invalid pair
    Then pair error codes are stable

  Scenario: Reject unavailable cidx capabilities
    Given a valid paired facts and project database
    When I request devirtualized traversal
    Then E_CAPABILITY is raised without fallback
