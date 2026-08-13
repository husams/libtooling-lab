Feature: Stable symbol identity
  Symbols use deterministic per-file identifiers and repeated declarations reuse USRs.

  Scenario: Symbol identifiers are allocated sequentially per file
    Given realistic shared-header declarations, definitions, parameters, and relations
    When the real facts-tool indexes both translation units using compile_commands.json
    Then every file has sequential zero-based symbol indices
    And every SymbolId packs its FileId and file index
    And every symbol allocator points to the next file index

  Scenario: Repeated declarations share one logical identity
    Given realistic shared-header declarations, definitions, parameters, and relations
    When the real facts-tool indexes both translation units using compile_commands.json
    Then these repeated declarations have one nonempty USR each
      | qualified_name    |
      | e2e               |
      | e2e::Deferred     |
      | e2e::Payload      |
      | e2e::Policy       |
      | e2e::Widget       |
      | e2e::headerHelper |
      | e2e::transform    |
    And no nonempty USR identifies more than one stored symbol
