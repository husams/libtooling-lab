Feature: Generated SDK operations follow the OpenAPI contract
  Scenario Outline: Download the documented YAML contract
    Given a native facts-tool REST server
    And an authenticated <kind> SDK client
    When I download the YAML and JSON contracts through generated operations
    Then both contract formats advertise the async job and command operations

    Examples:
      | kind  |
      | sync  |
      | async |

  Scenario: Async contract downloads progress during a blocked import
    Given a native facts-tool REST server
    And an authenticated async SDK client
    And a C++ project with an answer function
    And the project was imported through the SDK
    And another connection holds the project database lock
    And an import job is running and waiting for the database lock
    When I download the contract while polling the blocked import
    Then the contract and health respond before the import is released
