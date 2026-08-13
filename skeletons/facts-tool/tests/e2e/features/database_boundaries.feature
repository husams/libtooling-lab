Feature: Facts and file storage boundaries
  The file registry remains independent from extracted semantic facts.

  Background:
    Given realistic shared-header declarations, definitions, parameters, and relations
    When the real facts-tool indexes both translation units using compile_commands.json

  Scenario: File identity and captured facts use separate databases
    Then file identity and captured facts remain in separate databases
