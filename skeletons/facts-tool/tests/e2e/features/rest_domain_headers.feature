Feature: Registered header analysis through source identities
  Headers reuse validated including translation units without client compiler settings.

  Background:
    Given two real repositories with separate extracted fact databases

  Scenario Outline: A registered header uses its including translation unit
    Given the repository-aware server is running
    When I request typed "<operation>" analysis of the registered header
    Then the structured result identifies the requested header
    And header matcher results exclude declarations from the including source

    Examples:
      | operation    |
      | extractions  |
      | matches      |
      | dependencies |

  Scenario Outline: Header compilation context is validated across includers
    Given the header has two "<kind>" including compilation contexts
    And the repository-aware server is running
    When I request header extraction with multiple including contexts
    Then the header analysis reports "<outcome>"

    Examples:
      | kind        | outcome                       |
      | equivalent  | succeeded                     |
      | conflicting | ambiguous_compilation_context |

  Scenario: A registered header with no remaining includer reports missing context
    Given the repository-aware server is running
    When I remove the header include and request typed header extraction
    Then the header analysis reports "compilation_context_unavailable"

  Scenario: An inactive clone header uses its own source paths
    Given alpha has a registered inactive clone with different source content
    And the repository-aware server is running
    When I request matching in the inactive clone header
    Then matching reports the inactive header and leaves the active clone unchanged
