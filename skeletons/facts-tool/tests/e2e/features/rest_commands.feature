Feature: Native REST command interface
  Users and agents execute all CLI functionality through asynchronous HTTP jobs.

  Background:
    Given a running authenticated native REST server

  Scenario: Protected REST routes reject missing and invalid bearer tokens
    When requests omit the token or supply the wrong token
    Then every protected request is unauthorized and the server stays healthy

  Scenario: The catalog and OpenAPI expose every installed CLI command
    When I discover the REST command catalog and OpenAPI document
    Then every installed CLI command has a working REST help endpoint
    And OpenAPI describes all commands, authenticated requests, and job states

  Scenario Outline: Job records preserve native CLI output and exit status
    When I submit a REST CLI command with "<arguments>" arguments
    Then the REST job reports "<state>" with exit code <code> and "<text>"

    Examples:
      | arguments | state     | code | text        |
      | valid     | succeeded | 0    | conf_root   |
      | invalid   | failed    | 2    | usage error |

  Scenario: Concurrent clients receive independent job results
    When sixteen HTTP clients submit CLI jobs concurrently
    Then all jobs finish successfully with unique identities and available health

  Scenario: Import extraction matching and graph analysis use the real C++ pipeline
    Given a real C++ project imported and extracted through REST
    When I query symbols, run a matcher, and build a call graph through REST
    Then the REST results contain extracted functions, the match, and a complete graph

  Scenario: Cancelling native work preserves responsive HTTP service
    When a native CLI job is waiting for a database lock and another job is queued
    Then health responds while the native command is blocked
    When I cancel the queued and running REST jobs
    Then both jobs are cancelled and the server can run another CLI command
