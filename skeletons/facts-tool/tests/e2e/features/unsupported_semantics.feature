Feature: Typed pointer calls and destructor coverage are retained
  Extraction resolves provable local call targets and preserves destructor
  coverage without diagnosing trivial special members as failed CFGs.

  Scenario: initialized local function pointers retain exact call edges
    Given the semantic coverage fixture "local_function_pointers"
    When the semantic coverage fixture is extracted at verbosity zero
    Then initialized local function pointers have exact call edges
    And extraction emits no unsupported semantics diagnostics
    And repeated cached extraction preserves semantic facts and diagnostics

  Scenario: uncertain function pointers retain typed evidence without guessed targets
    Given the semantic coverage fixture "unresolved_function_pointers"
    When the semantic coverage fixture is extracted at verbosity zero
    Then uncertain function pointers have typed sites without guessed targets
    And extraction emits no unsupported semantics diagnostics
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

  Scenario: Pointer operands preserve declaration identity and full callable signatures
    Given the semantic coverage fixture "pointer_call_shapes"
    When the semantic coverage fixture is extracted at verbosity zero
    Then pointer call shapes preserve their typed sites and declaration relations
    And extraction emits no unsupported semantics diagnostics
    And repeated cached extraction preserves semantic facts and diagnostics

  Scenario: Pointer call entry queries preserve typed evidence without external identities
    Given the semantic coverage fixture "pointer_call_shapes"
    When the semantic coverage fixture is extracted at verbosity zero
    Then pointer call entries separate pointer evidence from external function targets
    And persisted pointer call runs contain only reached caller evidence

  Scenario: Version thirteen facts regenerate old unresolved sites as pointer calls
    Given the semantic coverage fixture "unresolved_function_pointers"
    When the semantic coverage fixture is extracted at verbosity zero
    And semantic pointer facts are downgraded to version thirteen unresolved evidence
    And the semantic coverage fixture is extracted at verbosity zero
    Then uncertain function pointers have typed sites without guessed targets
    And extraction emits no unsupported semantics diagnostics
    And repeated cached extraction preserves semantic facts and diagnostics

  Scenario: Changed bodies remove pointer evidence without deleting their value symbols
    Given the semantic coverage fixture "pointer_call_shapes"
    When the semantic coverage fixture is extracted at verbosity zero
    And the semantic pointer call expressions are removed and re-extracted
    Then obsolete pointer sites and relations are removed while value symbols survive
