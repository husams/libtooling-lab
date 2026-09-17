@ast_cache
Feature: Reuse persisted Clang ASTs across native commands
  AST persistence is optional and all AST consumers share configured entries.

  Background:
    Given an isolated AST cache project

  Scenario: Forced extraction reuses a serialized AST with identical semantic facts
    Given AST caching is enabled
    When the AST cache project runs "extract"
    Then the cold AST is persisted
    When the AST cache project runs "extract"
    Then the persisted AST is reused
    And cold and warm extraction facts are identical

  Scenario: Repeated current extraction skips AST loading and all frontend work
    Given AST caching is enabled
    And a persisted AST from extraction
    When current extraction runs twice without forcing
    Then current extraction reuses dependencies without loading or rebuilding an AST
    And cold and warm extraction facts are identical

  Scenario Outline: Native commands reuse persisted ASTs or project dependency metadata
    Given AST caching is enabled
    And a persisted AST from extraction
    When the AST cache project runs "<family>"
    Then the configured cache is reused by the command
    And the cached "<family>" result is complete

    Examples:
      | family        |
      | match         |
      | import        |
      | dependency    |
      | variable-flow |

  Scenario: AST persistence is disabled by default
    When the AST cache project runs "extract"
    Then no AST cache directory is created

  Scenario: Explicitly disabled AST persistence creates no cache
    Given AST caching is explicitly disabled
    When the AST cache project runs "extract"
    Then no AST cache directory is created

  Scenario: Disabling persistence bypasses an existing AST without touching it
    Given AST caching is enabled
    And a persisted AST from extraction
    When AST caching is disabled after the cache was populated
    Then the cache is untouched and emits no cache events
    And cold and warm extraction facts are identical

  Scenario: An enabled cache uses its project default directory
    Given AST caching is enabled
    When the AST cache project runs "extract"
    Then the default project AST cache directory is populated

  Scenario: A configured cache directory is used instead of the default
    Given AST caching uses a custom directory
    When the AST cache project runs "extract"
    Then only the configured AST cache directory is populated

  Scenario Outline: Disabling persistence bypasses the cache in every consumer
    Given AST caching is enabled
    And a persisted AST from extraction
    When AST caching is disabled and "<family>" runs against the populated cache
    Then the cache is untouched and emits no cache events
    And the cached "<family>" result is complete

    Examples:
      | family        |
      | match         |
      | import        |
      | dependency    |
      | variable-flow |

  Scenario: A persisted AST retains its compilation directory across CLI invocations
    Given an explicitly configured AST cache project with relative nested includes
    When the AST cache project runs "extract"
    Then the import-prepared AST is reused for the first extraction
    And the original nested include symbols and registered header paths are retained
    When warm extraction runs from another process directory using absolute selectors
    Then the persisted AST is reused
    And cold and warm extraction facts are identical
    And the original nested include symbols and registered header paths are retained
