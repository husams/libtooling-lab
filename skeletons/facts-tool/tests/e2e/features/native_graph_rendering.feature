Feature: One native call graph invocation produces a portable artifact
  Background:
    Given the S-025 two-component graph fixture

  Scenario: Mermaid output preserves cross-component graph evidence
    When S-025 recovers a Mermaid artifact
    Then the S-025 artifact contains the recovered graph and provenance
    And S-025 read-only output reuses that graph without changing its databases

  Scenario: Recovery failure retains a labelled partial diagram
    Given the S-025 library fails compilation
    When S-025 recovers a Mermaid artifact
    Then S-025 retains a valid partial artifact with a recovery error

  Scenario Outline: Each format produces one coherent final file
    When S-025 writes a <format> graph artifact
    Then S-025 has a coherent <format> file and empty stdout
    Examples:
      | format  |
      | text    |
      | json    |
      | mermaid |

  Scenario: Initial graph is published before recovery
    When S-025 observes the artifact during recovery
    Then S-025 observed a labelled initial graph and its final replacement

  Scenario Outline: Interrupting recovery retains the usable graph
    When S-025 interrupts <format> recovery during extraction
    Then S-025 retains a useful cancelled <format> result
    Examples:
      | format  |
      | mermaid |
      | json    |

  Scenario: JSON operational errors retain a single diagnostic channel
    When S-025 requests JSON from an empty facts database
    Then S-025 emits one JSON error without a duplicate stderr diagnostic

  Scenario Outline: Invalid requests preserve an existing artifact
    When S-025 requests an invalid <kind> graph artifact
    Then S-025 rejects the request without replacing the artifact
    Examples:
      | kind          |
      | root          |
      | target        |
      | configuration |
      | component     |

  Scenario Outline: Output cannot overwrite an input
    When S-025 writes over the <input> input
    Then S-025 rejects the output and preserves that input
    Examples:
      | input   |
      | facts   |
      | project |
      | source  |
      | alias   |

  Scenario: Mermaid keeps explicit node budgets
    When S-025 renders with a one-node budget
    Then S-025 renders the truncation frontier without claiming completion

  Scenario Outline: Optional query modes reach the renderer
    When S-025 renders a <mode> query
    Then S-025 retains the <mode> query metadata
    Examples:
      | mode    |
      | callers |
      | path    |

  Scenario: Unchanged recovery traverses one graph generation
    When S-025 repeats recovery of an already recovered artifact
    Then S-025 traverses once and reports no extraction attempts

  Scenario: Configured project and facts defaults support the native invocation
    When S-025 renders using configured pair defaults
    Then the S-025 artifact contains the recovered graph and provenance

  Scenario Outline: Explicit budgets apply to optional query modes
    When S-025 renders a budgeted <mode> query
    Then S-025 renders the truncation frontier without claiming completion
    Examples:
      | mode    |
      | callers |
      | path    |
