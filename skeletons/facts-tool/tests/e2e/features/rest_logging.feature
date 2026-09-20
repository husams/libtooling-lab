Feature: Configurable asynchronous server logging
  Server configuration and log output are independent deployment concerns.

  Scenario Outline: Foreground and daemon processes log to the configured destination
    Given a native REST <mode> with a separate log file
    When I complete a successful and a failed CLI job through REST
    Then the separate log reports readiness and the complete job lifecycle
    And the log destination and level are saved separately from server configuration
    When I restart the logged server from its saved configuration
    Then new lifecycle records are appended without losing the previous run

    Examples:
      | mode   |
      | server |
      | daemon |

  Scenario Outline: Numeric verbosity selects the requested logging detail
    Given a native REST server with verbosity <verbosity>
    When I complete a successful and a failed CLI job through REST
    Then only events at "<level>" or above are recorded

    Examples:
      | verbosity | level |
      | 0         | error |
      | 1         | info  |
      | 2         | debug |
      | 3         | trace |

  Scenario: Logging preserves responsiveness while native work waits for a database
    Given a native REST server with verbosity 3
    When a native CLI job is waiting for a database lock and another job is queued
    Then health responds while the native command is blocked
    When I cancel the queued and running REST jobs
    Then both jobs are cancelled and the server can run another CLI command

  Scenario: Detailed request logging excludes secrets and command output
    Given a native REST server with verbosity 3
    When I send secrets in REST credentials, arguments, paths, and request bodies
    Then HTTP and job records are present without any secret values

  Scenario: CLI log settings override and replace saved YAML defaults
    Given saved server logging defaults
    When I start the server with CLI logging overrides
    Then the chosen file and verbosity override the YAML and survive restart
