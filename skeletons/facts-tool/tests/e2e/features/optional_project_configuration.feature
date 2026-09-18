Feature: Project database options are optional throughout the command tree
  Scenario Outline: Every catalog operation reuses the automatically selected database
    Given a catalog configured through "<tier>" without command-line database options
    When I manage repositories components directories files and matched indexes without database options
    Then every catalog operation used the automatically configured project database
    Examples:
      | tier    |
      | project |
      | user    |
      | env     |
      | db-env  |

  Scenario: Every command leaf advertises optional configuration overrides
    Given an isolated defaults project
    When I inspect configuration options on every command leaf
    Then every leaf accepts omitted configuration and rejects explicitly empty overrides
