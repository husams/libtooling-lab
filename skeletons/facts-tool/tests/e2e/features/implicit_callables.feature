Feature: Compiler-provided callable identity
  Scenario: Filter system header call sites before resolving their targets
    Given fresh and populated traversal fixtures
    When the same traversal source is extracted against both catalog states
    Then excluded header sites neither resolve targets nor contribute runtime calls

  Scenario Outline: Classify and persist every explicit callable in the toolchain matrix
    Given a compiler callable matrix "<fixture>" with <count> explicit calls
    When the callable matrix is extracted and reopened
    Then every matrix call follows its observed USR and declaration provenance

    Examples:
      | fixture                        | count |
      | implicit_allocation_matrix.cpp | 12    |
      | implicit_nothrow_matrix.cpp    | 9     |

  Scenario: Reuse a locationless target across TUs and reopened extraction
    Given two implicit allocation translation units
    When both implicit translation units are repeatedly extracted
    Then the target identity and both real call sites remain stable

  Scenario: Preserve a committed database when a new compiler target cannot be saved
    Given an implicit allocation fixture
    When a new implicit target write fails after a committed baseline
    Then the target failure reports its identity and rolls back every facts table

  Scenario: Preserve primitive identities and occupied dynamic IDs in both orders
    Given an implicit allocation fixture
    When compiler allocation storage invariants are exercised
    Then all primitive and dynamic storage invariants hold
