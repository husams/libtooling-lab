Feature: Facts consumers use the configured project without a project argument
  Scenario Outline: <command> discovers <tier> configuration with <output> facts
    Given a facts consumer project configured through "<tier>"
    When configured consumer "<command>" runs with "<output>" facts
    Then the consumer uses the configured project and selected facts
    Examples:
      | command          | tier        | output   |
      | match            | project     | default  |
      | match            | project     | explicit |
      | match            | user        | explicit |
      | match            | environment | explicit |
      | match            | builtin     | explicit |
      | call-graph       | project     | default  |
      | call-graph       | project     | explicit |
      | call-graph       | user        | explicit |
      | call-graph       | environment | explicit |
      | call-graph       | builtin     | explicit |
      | call-graph-entry | project     | default  |
      | call-graph-entry | project     | explicit |
      | call-graph-entry | user        | default  |
      | call-graph-entry | environment | default  |
      | call-graph-entry | builtin     | explicit |
      | recovery         | project     | default  |
      | recovery         | project     | explicit |
      | recovery         | user        | explicit |
      | recovery         | environment | explicit |
      | recovery         | builtin     | explicit |
      | symbol           | project     | default  |
      | symbol           | project     | explicit |
      | symbol           | user        | default  |
      | symbol           | environment | explicit |
      | symbol           | builtin     | explicit |

  Scenario Outline: <command> never falls back from a missing configured project
    Given a facts consumer project configured through "project"
    And its configured project database is missing
    When configured consumer "<command>" runs with "explicit" facts
    Then the consumer reports the missing configured project without mutation
    Examples:
      | command          |
      | match            |
      | call-graph       |
      | call-graph-entry |
      | recovery         |
      | symbol           |

  Scenario Outline: Empty facts overrides are rejected for <command>
    Given a facts consumer project configured through "project"
    When configured consumer "<command>" runs with "empty" facts
    Then the consumer rejects the empty facts override without mutation
    Examples:
      | command          |
      | call-graph       |
      | call-graph-entry |

  Scenario Outline: An explicit project override wins for <command>
    Given a facts consumer project configured through "project"
    And its YAML project path is replaced with a missing database
    When configured consumer "<command>" runs with its original project override
    Then the consumer uses the configured project and selected facts
    Examples:
      | command          |
      | match            |
      | call-graph       |
      | call-graph-entry |
      | recovery         |
      | symbol           |
