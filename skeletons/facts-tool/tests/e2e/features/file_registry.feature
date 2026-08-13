Feature: Canonical source file registry
  Captured symbols refer only to canonical files imported before extraction.

  Scenario: Every symbol uses a preimported file identity
    Given realistic shared-header declarations, definitions, parameters, and relations
    When the real facts-tool indexes both translation units using compile_commands.json
    Then the file registry contains these canonical fixture paths
      | fixture   |
      | shared.hpp |
      | one.cpp    |
      | two.cpp    |
    And every registered FileId is greater than zero
    And every captured symbol uses a registered nonzero FileId
