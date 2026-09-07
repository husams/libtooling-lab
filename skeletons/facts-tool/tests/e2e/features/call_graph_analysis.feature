Feature: Contextual function call graph analysis
  Call sites retain canonical ownership and conservative receiver context.

  Scenario: records direct function method lambda constructor-body and constructor-invocation Calls once
    Given the exact contextual call graph corpus is extracted
    Then direct method lambda constructor-body and constructor-invocation Calls are recorded once

  Scenario: links a declaration-only CallRecord callee in one TU to its definition in another
    Given the exact contextual call graph corpus is extracted
    Then the cross-TU declaration-only callee resolves to its definition

  Scenario: preserves inherited method ownership with Calls and Overrides
    Given the exact contextual call graph corpus is extracted
    Then inherited calls and overrides retain canonical owners

  Scenario: resolves inherited MessageX DispatchCalls exactly in text
    Given the exact contextual call graph corpus is extracted
    Then MessageX dispatch is exact in storage and text

  Scenario: resolves inherited MessageY DispatchCalls exactly in text
    Given the exact contextual call graph corpus is extracted
    Then MessageY dispatch is exact in storage and text

  Scenario: keeps an unproven concrete receiver conservative as possible in text
    Given the possible-receiver contextual call graph corpus is extracted
    Then unproven receiver dispatch is possible and conservative

  Scenario: normalizes instantiated callers and sites to the written template pattern before deduplication
    Given the template contextual call graph corpus is extracted
    Then instantiated callers normalize to the written pattern

  Scenario: migrates version 7 to 8 by adding two nullable relation_site columns and no table
    Given the call graph migration regression is run
    Then the call graph migration regression passes

  Scenario: selects roots by qualified name and USR with optional depth in text
    Given the exact contextual call graph corpus is extracted
    Then name USR and positive-depth root selection agree

  Scenario: excludes the virtual root and analyzes all callables with byte-stable canonical ordering
    Given the exact contextual call graph corpus is extracted
    Then all-mode output is byte stable canonically ordered and excludes the virtual root

  Scenario: reports invalid context incomplete TUs and invalid explicit depth in text
    Given the exact contextual call graph corpus is extracted
    Then invalid graph state database and depth requests are diagnosed

  Scenario: enforces documented CallGraph node edge receiver and extractor ownership boundaries
    Given the call graph architecture regression is run
    Then the call graph architecture regression passes

  Scenario: records complete call graph validation evidence from text probes
    Given the exact contextual call graph corpus is extracted
    Then representative deterministic text fields are present

  Scenario: collapses multiple template receiver contexts at the normalized written site to NULL/possible while retaining DispatchCalls targets
    Given the template contextual call graph corpus is extracted
    Then template receiver contexts collapse while dispatch targets remain

  Scenario: stops default traversal at an external symbol and reports external-boundary without truncation in text
    Given the exact contextual call graph corpus is extracted
    Then default traversal reports a complete external boundary

  Scenario: applies an explicit positive depth cap before an external boundary and reports depth truncation distinctly in text
    Given the exact contextual call graph corpus is extracted
    Then explicit depth truncation is distinct from an external boundary

  Scenario: reproduces stored traversal completion at an unextracted project-local definition boundary
    Given an isolated B-040 pair has a declaration-only project boundary
    Then B-040 reports complete traversal and incomplete extraction coverage separately

  Scenario: reports simulated complete catalog metadata through a validated project and facts pair
    Given an isolated B-040 pair has complete known project coverage
    Then B-040 reports complete paired coverage with usable paths and stable identities

  Scenario: keeps genuinely unavailable definitions separate from project extraction gaps
    Given an isolated B-040 pair has complete known project coverage
    Then B-040 reports the genuine external definition boundary separately

  Scenario: reports missing catalog coverage metadata without blindly re-extracting known facts
    Given an isolated B-040 pair has missing coverage metadata
    Then B-040 reports unknown coverage and metadata reconciliation

  Scenario: reports simulated stale catalog evidence independently from stored graph traversal
    Given an isolated B-040 pair has stale coverage metadata
    Then B-040 reports stale coverage with a focused refresh action

  Scenario: keeps explicit depth truncation separate from extraction coverage
    Given an isolated B-040 pair has a declaration-only project boundary
    Then B-040 reports depth truncation independently in structured output
