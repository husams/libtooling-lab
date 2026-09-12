Feature: Located matcher results for Python processing
  A successful invocation returns its own matches after committing all selected
  translation units, with coordinates and explicit translation-unit provenance.

  Scenario Outline: JSON bindings can be processed by the public Python SDK
    Given an isolated two-source match results fixture
    When a structured matcher runs for "<contract>"
    Then the SDK reads located "<contract>" bindings for both translation units
    And matching reports each translation unit once

    Examples:
      | contract   |
      | symbol     |
      | call       |
      | relation   |
      | expression |

  Scenario: Default matcher text includes source coordinates
    Given an isolated two-source match results fixture
    When a located symbol matcher runs in text mode
    Then symbol output contains its header path and coordinates

  Scenario: An empty match produces an empty complete result collection
    Given an isolated two-source match results fixture
    When a structured matcher runs for "empty"
    Then the SDK reads an empty successful match collection

  Scenario: Macro and remapped coordinates identify the physical file
    Given a matcher fixture with macro remapping and implicit allocation
    When the macro symbol is returned as JSON
    Then remapped coordinates identify the physical expansion

  Scenario: Unsupported implicit locations fail without a success document
    Given a matcher fixture with macro remapping and implicit allocation
    When an unsupported implicit symbol is matched
    Then the location failure is explicit and publishes no JSON

  Scenario Outline: Text locations identify matched occurrences
    Given an isolated two-source match results fixture
    When a located "<contract>" matcher runs in text mode
    Then text output identifies the occurrence in each source

    Examples:
      | contract |
      | call     |
      | uses     |

  Scenario: A later parse failure publishes no successful JSON or new symbols
    Given an isolated two-source match results fixture
    And a structured matcher runs for "symbol"
    When a later translation unit fails after an earlier new symbol matches
    Then matching fails without publishing JSON or the earlier new symbol

  Scenario: Registration validation runs even when there are no matches
    Given an isolated two-source match results fixture
    And a structured matcher runs for "symbol"
    When a nonmatching source includes a new unregistered header
    Then matching fails with incomplete registration and preserves prior symbols

  Scenario Outline: Relative filename collisions remain separate during matching
    Given an isolated match fixture with colliding relative source paths
    When structured matching runs in "<order>" order
    Then both component-specific symbols and source paths are returned

    Examples:
      | order       |
      | large-small |
      | small-large |
