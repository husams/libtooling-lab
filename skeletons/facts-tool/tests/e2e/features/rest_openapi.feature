Feature: YAML-driven asynchronous REST contract
  Users and agents consume one documented OpenAPI contract for native requests.

  Background:
    Given a running authenticated native REST server

  Scenario: JSON and YAML expose the same complete validated OpenAPI contract
    When I download the live JSON and YAML API contracts
    Then both documents are valid OpenAPI and describe every registered CLI endpoint

  Scenario Outline: Generated command routes accept standard encoded and legacy paths
    When I submit the configuration command through the "<path>" route
    Then its accepted response and completed job match the OpenAPI contract

    Examples:
      | path          |
      | config%2Fshow |
      | config/show   |

  Scenario: Native success and failure payloads conform to their published schemas
    When I exercise discovery, invalid requests, and a failing CLI job
    Then every response matches its declared OpenAPI response schema

  Scenario: Serving generated metadata remains asynchronous during blocked work
    When a native CLI job is waiting for a database lock and another job is queued
    Then health responds while the native command is blocked
    When I download the live JSON and YAML API contracts
    Then the native CLI job is still running while metadata responds
    When I cancel the queued and running REST jobs
    Then both jobs are cancelled and the server can run another CLI command
    And both documents are valid OpenAPI and describe every registered CLI endpoint
