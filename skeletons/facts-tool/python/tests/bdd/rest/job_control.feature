Feature: Python REST clients control real queued and running processes
  Background:
    Given a native facts-tool REST server

  Scenario Outline: Cancel a queued job without affecting the running import
    Given an authenticated <kind> SDK client
    And a C++ project with an answer function
    And the project was imported through the SDK
    And another connection holds the project database lock
    And an import job is running and waiting for the database lock
    When I cancel a configuration job queued behind the blocked import
    Then the cancelled job never starts and the active import remains running
    When the other connection releases the database lock
    Then the original import completes successfully

    Examples:
      | kind  |
      | sync  |
      | async |

  Scenario Outline: Cancel a running native import
    Given an authenticated <kind> SDK client
    And a C++ project with an answer function
    And the project was imported through the SDK
    And another connection holds the project database lock
    And an import job is running and waiting for the database lock
    When I cancel the running import
    Then the import becomes cancelled and the next command can complete

    Examples:
      | kind  |
      | sync  |
      | async |

  Scenario Outline: A polling deadline leaves remote work running
    Given an authenticated <kind> SDK client
    And a C++ project with an answer function
    And the project was imported through the SDK
    And another connection holds the project database lock
    And an import job is running and waiting for the database lock
    When my deadline expires while polling the running import
    Then the timeout identifies the job and does not cancel remote work
    When the other connection releases the database lock
    Then the original import completes successfully

    Examples:
      | kind  |
      | sync  |
      | async |
