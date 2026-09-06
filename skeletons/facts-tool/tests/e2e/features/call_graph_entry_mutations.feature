Feature: Entry lifecycle across native commands
  Scenario: Catalog mutations invalidate the configured facts store
    Given an extracted S-027 application component
    Then S-027 catalog reads and dry runs preserve entries
    And S-027 catalog option edits invalidate configured entries

  Scenario: Import invalidates a facts store selected through configuration
    Given an extracted S-027 application component
    Then S-027 import without explicit facts invalidates configured entries

  Scenario: Catalog mutations stop when paired invalidation fails
    Given an extracted S-027 application component
    Then S-027 failed catalog invalidation leaves project options unchanged

  Scenario: One extraction over several translation units retains every caller
    Given an extracted S-027 application component
    Then S-027 multi-source extraction retains shared callers and resolved targets

  Scenario: A narrow external call match retains site evidence without an entry
    Given an extracted S-027 application component
    Then S-027 narrow external calls retain references without certifying a body

  Scenario Outline: Source-based facts templates invalidate every existing store
    Given an extracted S-027 application component
    Then S-027 "<operation>" invalidates all source-based configured stores

    Examples:
      | operation |
      | import    |
      | catalog   |

  Scenario: Dependency facts invalidate entries atomically
    Given an extracted S-027 application component
    Then S-027 dependency writes invalidate entries and failures roll back

  Scenario: Mutation facts paths cannot overwrite their project configuration
    Given an extracted S-027 application component
    Then S-027 mutations reject colliding or empty facts paths without writes

  Scenario: Project mutations require an identifiable facts pair
    Given an extracted S-027 application component
    Then S-027 mutations without a known facts pair stop before changing either store
