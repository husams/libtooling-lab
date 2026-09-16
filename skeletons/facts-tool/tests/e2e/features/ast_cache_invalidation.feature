@ast_cache
Feature: AST cache commit changes and failure recovery
  New Git commits or compiler contexts refresh cached source semantics, and unusable entries
  must never prevent an otherwise valid command from completing.

  Background:
    Given an isolated AST cache project
    And AST caching is enabled

  Scenario Outline: A changed commit or compiler context rebuilds the persisted AST
    Given a persisted AST from extraction
    When the AST cache input "<input>" changes
    Then AST regeneration exposes the fresh symbol "<symbol>"

    Examples:
      | input                                           | symbol               |
      | source commit                                   | cache_source_changed |
      | header commit                                   | CacheHeaderChanged   |
      | compiler arguments                              | cache_mode_enabled   |
      | header commit with preserved size and timestamp | HeaderAfter_         |

  Scenario: A corrupt serialized AST is reparsed and replaced
    Given a persisted AST from extraction
    When the persisted AST bytes are corrupted and extraction runs
    Then the corrupt AST is rebuilt and reusable

  Scenario: An unusable cache directory falls back to normal parsing
    Given the configured cache directory is blocked by a regular file
    When the AST cache project runs "extract"
    Then extraction succeeds without damaging the blocking file
