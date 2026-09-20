Feature: Monitor configured repositories from the project database
  Repository registration is the source of truth for background monitoring.

  Background:
    Given two indexed repositories registered with custom names and clone labels

  Scenario: Default monitoring discovers all active repository clones
    Given the catalog server starts without a monitored directory list
    When I edit sources in both configured repositories
    Then both repositories publish fresh symbols without changing catalog identities

  Scenario: Published OpenAPI describes active and excluded repository clones
    Given the catalog server excludes beta by "repository"
    Then the repository watch status conforms to the published OpenAPI schema

  Scenario Outline: Repository and clone exclusions scope automatic work
    Given the catalog server excludes beta by "<selector>"
    When I edit beta and then alpha
    Then only alpha is monitored and beta keeps its imported and indexed state

    Examples:
      | selector        |
      | repository      |
      | clone label     |
      | qualified clone |
      | clone path      |
