Feature: Configuration defaults

  Scenario: Show YAML defaults without creating a database
    Given a YAML defaults file with one compiler token
    When I show configuration defaults from that directory
    Then configuration output names the YAML source and compiler token
    And configuration inspection creates no generated database
