Feature: Committed entries into the shared function graph
  Scenario: Shared callees, cycles and generated leaves retain one identity
    Given an extracted S-027 application component
    Then S-027 entries reuse shared nodes and preserve generated leaves
    And repeated S-027 extraction is idempotent

  Scenario: Narrow matches invalidate entries without certifying whole bodies
    Given an extracted S-027 application component
    When an S-027 narrow call match runs
    Then the S-027 caller entry is missing until full regeneration

  Scenario: A known external target resolves in another registered component
    Given an extracted S-027 application component
    Then the S-027 external call retains its identity and site
    When the S-027 library component is extracted
    Then the S-027 external identity resolves without losing callers or sites

  Scenario: Indirect calls and entry availability do not imply completeness
    Given an extracted S-027 application component
    Then S-027 indirect calls have no guessed external identity
    And S-027 freshness and graph truncation remain separate from entries

  Scenario: Entry lookup reports paired complete coverage
    Given an extracted S-027 application component
    When S-027 catalog metadata marks the entry source complete
    Then S-027 entry lookup reports validated complete coverage

  Scenario: Entry aggregate coverage includes reachable missing definitions
    Given an extracted S-027 application component
    When S-027 catalog metadata marks the entry source complete
    Then S-027 entry aggregate coverage reports the missing library definition

  Scenario: Entry lookup reports stale paired coverage
    Given an extracted S-027 application component
    When S-027 catalog metadata marks the entry source stale
    Then S-027 entry lookup reports stale coverage with a refresh action

  Scenario: Entry lookup without a project keeps coverage unknown
    Given an extracted S-027 application component
    Then S-027 entry lookup without project configuration keeps coverage unknown

  Scenario: Entry publication failure rolls back and can be retried
    Given an extracted S-027 application component
    When S-027 entry publication is forced to fail
    Then committed S-027 graph entries survive the rollback and retry

  Scenario: Reimport invalidates entries before committing changed inputs
    Given an extracted S-027 application component
    When the S-027 source command is changed and reimported
    Then the S-027 caller entry is missing until full regeneration

  Scenario: Failed paired invalidation refuses a project mutation
    Given an extracted S-027 application component
    Then failed S-027 invalidation preserves the prior project commands

  Scenario: Version ten migration starts with no inferred entries
    Given an extracted S-027 application component
    Then S-027 version ten migration preserves graph identities without inferring entries

  Scenario: Unknown newer schemas reject writes
    Given an extracted S-027 application component
    Then S-027 future facts versions reject extraction without writes

  Scenario: Entry lookup is read only and rejects ambiguous or unknown roots
    Given an extracted S-027 application component
    Then S-027 lookup preserves database bytes and rejects invalid selectors

  Scenario: Extract never writes the four-field matched symbol index
    Given an extracted S-027 application component
    Then S-027 extraction leaves the guarded matched symbol index unchanged

  Scenario: Entry and external reference schemas enforce graph node and site keys
    Given an extracted S-027 application component
    Then S-027 tables have exactly their contracted columns and cascading references

  Scenario: Independent imported stores cannot join overlapping numeric IDs
    Given an extracted S-027 application component
    Then S-027 mismatched project and facts stores reject overlapping raw IDs

  Scenario: Regenerated function bodies replace obsolete call evidence
    Given an extracted S-027 application component
    Then S-027 regenerated bodies replace obsolete calls and unresolved sites
