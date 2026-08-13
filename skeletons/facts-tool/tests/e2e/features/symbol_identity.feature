Feature: Stable symbol identity
  Symbols use deterministic per-file identifiers and repeated declarations reuse USRs.

  Background:
    Given realistic shared-header declarations, definitions, parameters, and relations
    When the real facts-tool indexes both translation units using compile_commands.json

  Scenario: Symbol identifiers are allocated sequentially per file
    Then SymbolIds are packed from stable, sequential per-file indices

  Scenario: Repeated declarations share one logical identity
    Then repeated declarations and translation units reuse each USR's SymbolId
