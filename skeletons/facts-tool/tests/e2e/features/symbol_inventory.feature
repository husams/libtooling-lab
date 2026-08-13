Feature: Extracted symbol and fact inventory
  Supported declarations, definitions, parameters, and references are captured together.

  Background:
    Given realistic shared-header declarations, definitions, parameters, and relations
    When the real facts-tool indexes both translation units using compile_commands.json

  Scenario: Supported declarations use their concrete node kinds
    Then concrete supported symbol types are present

  Scenario: Function definitions and parameter metadata are captured
    Then function definitions, parameter names, and defaults are present

  Scenario: Typed facts and relations retain referential integrity
    Then typed facts and relations reference captured symbols
