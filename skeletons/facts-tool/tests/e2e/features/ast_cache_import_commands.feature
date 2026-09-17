@ast_cache
Feature: Import prepares the exact compiler context used by later commands
  Duplicate commands and compiler defaults resolve before AST preparation.
  Only the selected stored command owns a cache entry for each source.

  Background:
    Given an isolated AST cache project
    And AST caching is enabled

  Scenario Outline: Duplicate commands share the stored winner and normalized cache key
    Given duplicate compilation commands with removable flags in "<order>" order
    When the AST cache project runs "import"
    Then import prepares one AST for the deterministic stored command
    When the first imported-context "<family>" runs
    Then the imported compiler context is reused with its selected declaration

    Examples:
      | order           | family  |
      | winner first    | extract |
      | winner last     | extract |
      | winner first    | match   |
      | winner last     | match   |

  Scenario Outline: YAML compiler defaults are shared by import and its first consumer
    Given YAML compiler defaults select the cached declaration
    When the AST cache project runs "import"
    Then import prepares the configured compiler context
    When the first imported-context "<family>" runs
    Then the imported compiler context is reused with its selected declaration

    Examples:
      | family  |
      | extract |
      | match   |

  Scenario: CLI compiler overrides take precedence when preparing the imported AST
    Given YAML compiler defaults select the cached declaration
    When import overrides the configured compiler declaration
    Then import prepares the overridden compiler context once

  Scenario Outline: Standalone forced headers are registered while preparing the AST
    Given a standalone compiler forced header outside the source and include search roots
    When the AST cache project runs "import"
    Then import prepares the configured compiler context
    And the standalone forced header has a registered file identity
    When the first imported-context "<family>" runs
    Then the imported compiler context is reused with its selected declaration

    Examples:
      | family  |
      | extract |
      | match   |
