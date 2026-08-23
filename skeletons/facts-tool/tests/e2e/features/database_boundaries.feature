Feature: Facts and file storage boundaries
  The file registry remains independent from extracted semantic facts.

  Background:
    Given realistic shared-header declarations, definitions, parameters, and relations
    When the real facts-tool indexes both translation units using compile_commands.json

  Scenario: File identity and captured facts use separate databases
    Then the facts database contains these tables
      | table            |
      | symbol           |
      | symbol_allocator |
      | definition       |
      | parameter        |
      | relation         |
    And the facts database excludes the file table
    And no facts table stores opaque packed flags
    And the files database contains only these tables
      | table             |
      | clone             |
      | component         |
      | directory         |
      | file              |
      | include_dependency |
      | repository        |
      | semantic_universe |
    And the facts and files databases use different paths
