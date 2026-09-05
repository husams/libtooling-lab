Feature: Facts and project catalogs
  Background:
    Given a valid paired facts and project database

  Scenario: Publish every relation kind
    Then all 23 persisted relation mappings are available

  Scenario: Query ordered parameters and defaults
    When I query run parameters
    Then the stored parameter and default are preserved

  Scenario: Use conventional template terminology
    When I query template slots and supplied arguments
    Then physical template names are mapped conventionally

  Scenario: Query edge occurrence evidence
    When I query call relation sites
    Then repeated sites and full relation keys are preserved

  Scenario: Resolve project files and includes
    When I query project files and include edges
    Then both project metadata and facts include data are used

  Scenario: Explain without running rows
    When I explain a call query
    Then both database identities budgets and catalogs are reported
