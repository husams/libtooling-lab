Feature: Canonical source file registry
  Captured symbols refer only to canonical files imported before extraction.

  Background:
    Given realistic shared-header declarations, definitions, parameters, and relations
    When the real facts-tool indexes both translation units using compile_commands.json

  Scenario: Every symbol uses a preimported file identity
    Then every source path is canonical and every symbol uses a preimported FileId
