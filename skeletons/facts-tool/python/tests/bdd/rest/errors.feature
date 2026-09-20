Feature: Python REST clients preserve server and command failures
  Background:
    Given a native facts-tool REST server

  Scenario Outline: Reject invalid authentication
    Given an unauthenticated <kind> SDK client
    When I request health without valid authentication
    Then the SDK reports HTTP 401 with the server error

    Examples:
      | kind  |
      | sync  |
      | async |

  Scenario Outline: Inspect a failed native command
    Given an authenticated <kind> SDK client
    When I run an invalid CLI command with error checking
    Then JobFailedError preserves the exit code and native usage error
    When I repeat the invalid command with error checking disabled
    Then the SDK returns the failed job without raising

    Examples:
      | kind  |
      | sync  |
      | async |

  Scenario Outline: Report an unknown job
    Given an authenticated <kind> SDK client
    When I request a job which does not exist
    Then the SDK reports HTTP 404

    Examples:
      | kind  |
      | sync  |
      | async |

  Scenario Outline: Shut down a real server
    Given an authenticated <kind> SDK client
    When I stop the server through the SDK
    Then shutdown is acknowledged and the HTTP listener closes

    Examples:
      | kind  |
      | sync  |
      | async |
