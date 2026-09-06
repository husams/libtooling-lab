Feature: Matched symbol index
  Successful matching maintains a four-field cross-component discovery index
  without adding index work to extraction.

  Scenario: Project schema migration creates the exact empty index
    Given an S-026 matcher project pair
    Then the project has version one and the exact four-field matched index

  Scenario: Version-zero project migration preserves facts metadata
    Given an S-026 matcher project pair
    And the S-026 project is reset to schema version zero
    When import reopens the S-026 version-zero project
    Then the project migration preserves facts user version and creates no candidates

  Scenario: Unknown newer project schemas reject without writes
    Given an S-026 matcher project pair
    When an S-026 newer project schema is queried
    Then the unsupported project schema is reported without database writes

  Scenario: Repeated matching upserts one candidate
    Given an S-026 matcher project pair
    When the S-026 caller symbol is matched twice
    Then matched-symbol lookup returns one matched-only candidate

  Scenario: Every explicit symbol binding is indexed
    Given an S-026 matcher project pair
    When relation and direct-call symbol bindings are matched
    Then explicit relation and callee bindings are indexed without the derived caller

  Scenario: Literal name lookup crosses registered repositories
    Given an imported catalog with two repositories and independent components
    When symbols are matched in two independent catalog components
    Then matched-symbol lookup returns both repositories deterministically

  Scenario: Same names and USRs retain their file identities
    Given an S-026 duplicate identity project
    When all S-026 duplicate symbols are matched
    Then same-name USRs stay distinct and one USR keeps both files

  Scenario: Invalid symbol identity publishes no index row
    Given an S-026 invalid-USR matcher project
    When the S-026 invalid identity is matched
    Then matching reports invalid-usr and publishes no candidate

  Scenario: Unregistered header declarations use no invented file ID
    Given an S-026 matcher project pair
    When an S-026 unregistered header declaration is matched
    Then matching reports the incomplete project and publishes no candidate

  Scenario: Invalid clear file IDs are rejected without writes
    Given an S-026 matcher project pair
    And the S-026 caller symbol is already indexed
    When the matched index is cleared with invalid file IDs
    Then the invalid file ID is rejected without changing candidates

  Scenario: Empty name selectors are rejected
    Given an S-026 matcher project pair
    When matched-symbol lookup receives an empty name selector
    Then the empty matched-symbol selector is rejected

  Scenario: Unknown positive clear file IDs are no-ops
    Given an S-026 matcher project pair
    And the S-026 caller symbol is already indexed
    When the matched index is cleared with an unknown positive file ID
    Then the unknown clear succeeds without changing candidates

  Scenario: Extraction performs zero matched-index writes
    Given an S-026 matcher project pair
    And the S-026 caller symbol is already indexed
    When extraction runs against empty and populated facts under an index write guard
    Then extraction succeeds without changing matched candidates

  Scenario: Failed project publication reports committed facts and retries
    Given an S-026 matcher project pair
    When matched-index publication is forced to fail
    Then facts are committed without a false index row and retry succeeds

  Scenario: Combined-store publication is one transaction
    Given an S-026 matcher project pair
    When combined-store matched-index publication is forced to fail
    Then combined-store facts and index rows are both rolled back

  Scenario: Clear and file removal explicitly remove candidates
    Given an S-026 matcher project pair
    And the S-026 caller symbol is already indexed
    When its matched index file is cleared then rematched and removed
    Then no matched candidate remains for that file

  Scenario: Extraction timings are recorded for empty and populated indexes
    Given an S-026 matcher project pair
    When S-026 extraction timing runs one warm-up and five measurements per state
    Then the raw extraction timings and medians are recorded without a speed claim
