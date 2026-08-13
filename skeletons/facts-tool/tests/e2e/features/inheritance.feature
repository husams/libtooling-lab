Feature: C++ record inheritance
  Direct bases are stored with their order, access, and virtual inheritance state.

  Background:
    Given realistic shared-header declarations, definitions, parameters, and relations
    When the real facts-tool indexes both translation units using compile_commands.json

  Scenario: Direct inheritance produces typed symbol relations
    Then direct inheritance uses SymbolId relations with access and virtual flags
