@ast_cache
Feature: Git commits define the AST and dependency cache refresh boundary
  Uncommitted source changes deliberately reuse the committed cache generation.
  New commits and new compiler contexts refresh it, while missing artifacts
  and non-Git sources retain normal parsing fallback behavior.

  Background:
    Given an isolated AST cache project
    And AST caching is enabled

  Scenario Outline: Dirty source content at the same commit does not trigger reparsing
    Given a persisted AST from extraction
    When the uncommitted "<input>" becomes invalid and "<family>" runs
    Then the same commit reuses the original AST and semantic facts without rewriting its artifact

    Examples:
      | input  | family        |
      | source | extract       |
      | header | extract       |
      | source | match         |
      | header | match         |
      | source | variable-flow |
      | header | variable-flow |

  Scenario: Advancing Git HEAD refreshes the AST even when source contents are unchanged
    Given a persisted AST from extraction
    When Git HEAD advances without changing the translation unit inputs
    Then the new commit rebuilds the AST with unchanged semantic facts

  Scenario: Missing AST storage falls back to parsing without a separate dependency scan
    Given a persisted AST from extraction
    When the persisted AST file disappears before extraction
    Then the missing AST is rebuilt while dependency metadata is reused

  Scenario: Non-Git sources parse without creating persistent cache artifacts
    Given the source is outside any Git repository that tracks it
    When the AST cache project runs "extract"
    Then extraction succeeds repeatedly without persisting an AST for the non-Git source

  Scenario: Compiler time macros remain eligible for reuse at the same commit
    Given the committed translation unit uses compiler time macros
    And a persisted AST from extraction
    When the uncommitted "source" becomes invalid and "extract" runs
    Then the same commit reuses the original AST and semantic facts without rewriting its artifact

  Scenario: A new commit in a dependency repository refreshes the source AST
    Given a committed header is included from a separate Git repository
    And a persisted AST from extraction
    When only the external header repository advances to a new commit
    Then AST regeneration exposes the fresh symbol "CacheExternalCommitted"

  Scenario: Committing an initially untracked dependency header refreshes the source AST
    Given an initially untracked header is included from another committed repository
    And a persisted AST from extraction
    When only the external header repository advances to a new commit
    Then AST regeneration exposes the fresh symbol "CacheExternalCommitted"

  Scenario: A searched repository commit adds a previously absent optional header
    Given a committed dependency repository is searched for an absent optional header
    And a persisted AST from extraction
    When a dependency repository commit adds the optional header and the registry is refreshed
    Then AST regeneration exposes the fresh symbol "CacheExternalOptionalCommitted"

  Scenario: Macro-wrapped compiler time text is preserved exactly in cached facts
    Given the committed translation unit wraps compiler time macros in a macro
    And a persisted AST from extraction
    When the uncommitted "source" becomes invalid and "extract" runs
    Then the same commit reuses the original AST and semantic facts without rewriting its artifact
