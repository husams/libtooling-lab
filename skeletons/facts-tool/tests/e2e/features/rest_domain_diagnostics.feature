Feature: Structured native analysis diagnostics
  Analysis failures expose compiler diagnostics without corrupting server logs.

  Scenario Outline: Successful native analysis preserves structured daemon logs
    Given two real repositories with separate extracted fact databases
    And a repository-aware daemon with a configured JSON log
    When I complete valid C++ analysis through the typed "<operation>" API
    Then every daemon log line remains a valid structured JSON event

    Examples:
      | operation   |
      | extractions |
      | matches     |

  Scenario Outline: Native compiler errors remain structured in daemon mode
    Given two real repositories with separate extracted fact databases
    And a repository-aware daemon with a configured JSON log
    When I submit invalid C++ through the typed "<operation>" API
    Then the failed domain job contains structured compiler diagnostics
    And every daemon log line remains a valid structured JSON event

    Examples:
      | operation   |
      | extractions |
      | matches     |
