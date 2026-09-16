@ast_cache
Feature: AST cache tracks compiler includes and header lookup decisions
  Forced includes are reused from project metadata. New Git commits refresh
  header lookup decisions, while compiler context changes select new entries.

  Background:
    Given an isolated AST cache project
    And AST caching is enabled

  Scenario Outline: Project cache metadata preserves compiler forced include dependencies
    Given the cached translation unit uses a compiler forced include
    And a persisted AST from extraction
    When the forced include consumer "<family>" runs with fresh output
    Then the configured cache is reused by the command
    And the cached include graph retains the compiler forced header

    Examples:
      | family     |
      | import     |
      | dependency |

  Scenario: Committing a newly shadowing header invalidates the prior include lookup
    Given an include resolves through a lower priority search directory
    And a persisted AST from extraction
    When a header appears in the higher priority search directory and the registry is refreshed
    Then the changed include lookup exposes "CacheSearchShadow"

  Scenario: Committing a previously absent conditional header invalidates the AST
    Given the source conditionally includes a header that does not exist
    And a persisted AST from extraction
    When the optional header becomes available and the registry is refreshed
    Then the changed include lookup exposes "CacheOptionalAvailable"

  Scenario: Reimporting changed response file options invalidates the persisted AST
    Given the compiler arguments come from a response file
    And a persisted AST from extraction
    When the compiler response file enables a different source declaration and is reimported
    Then AST regeneration exposes the fresh symbol "cache_mode_enabled"

  Scenario Outline: AST consumers reject stale valid ASTs after a committed compile error
    Given a persisted AST from extraction
    When the cached "<input>" acquires a committed compile error and "<family>" runs
    Then the command reports the fresh compile error instead of using the old AST

    Examples:
      | input  | family        |
      | source | match         |
      | header | match         |
      | source | variable-flow |
      | header | variable-flow |

  Scenario: Cache fingerprints preserve physical symlink followed by parent resolution
    Given an include search path traverses a symlink followed by its parent
    And a persisted AST from extraction
    Then the physically resolved header symbol is present
    When the AST cache project runs "extract"
    Then the persisted AST is reused
    And the physically resolved header symbol is present
    When the physical header behind the include path is committed without a timestamp change
    Then AST regeneration exposes the fresh symbol "CachePhysicalAfter_"
