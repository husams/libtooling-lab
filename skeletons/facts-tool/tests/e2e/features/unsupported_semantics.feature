Feature: Unsupported semantics diagnostics reflect genuine coverage gaps
  Extraction resolves provable local call targets and preserves destructor
  coverage without diagnosing trivial special members as failed CFGs.

  Scenario: initialized local function pointers retain exact call edges
    Given the semantic coverage fixture "local_function_pointers"
    When the semantic coverage fixture is extracted at verbosity zero
    Then initialized local function pointers have exact call edges
    And extraction emits no unsupported semantics diagnostics
    And repeated cached extraction preserves semantic facts and diagnostics

  Scenario: uncertain function pointers remain observable coverage gaps
    Given the semantic coverage fixture "unresolved_function_pointers"
    When the semantic coverage fixture is extracted at verbosity zero
    Then uncertain function pointers have diagnostics without guessed targets
    And repeated cached extraction preserves semantic facts and diagnostics

  Scenario: vector unique pointer cleanup is extracted without false diagnostics
    Given the semantic coverage fixture "standard_library_cleanup"
    When the semantic coverage fixture is extracted at verbosity zero
    Then vector and unique pointer cleanup edges are preserved
    And extraction emits no unsupported semantics diagnostics
    And repeated cached extraction preserves semantic facts and diagnostics

  Scenario: defaulted cleanup retains member base array and temporary edges
    Given the semantic coverage fixture "defaulted_cleanup"
    When the semantic coverage fixture is extracted at verbosity zero
    Then defaulted cleanup retains its nontrivial destructor edges
    And extraction emits no unsupported semantics diagnostics
    And repeated cached extraction preserves semantic facts and diagnostics
