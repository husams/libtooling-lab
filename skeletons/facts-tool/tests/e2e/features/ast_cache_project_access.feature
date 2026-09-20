@ast_cache
Feature: AST consumers persist cache data alongside project readers
  Commands that parse translation units can create and repair the configured AST
  cache while retaining the imported project registry.

  Background:
    Given an isolated AST cache project

  Scenario Outline: A cold AST consumer writes reusable project cache metadata
    Given AST caching is enabled
    When the AST cache project runs "<family>"
    Then the AST consumer persists valid project cache metadata
    And the cached "<family>" result is complete
    When the AST cache project runs "<family>"
    Then the persisted AST is reused
    And the cached "<family>" result is complete

    Examples:
      | family        |
      | extract       |
      | match         |
      | variable-flow |

  Scenario Outline: AST consumers repair corrupted artifacts and project metadata
    Given AST caching is enabled
    And a persisted AST from extraction
    And the serialized AST is "corrupt" but database metadata remains intact
    When the AST cache project runs "<family>"
    Then the AST consumer persists valid project cache metadata
    And the cached "<family>" result is complete
    When the AST cache project runs "<family>"
    Then the persisted AST is reused
    And the cached "<family>" result is complete

    Examples:
      | family        |
      | extract       |
      | match         |
      | variable-flow |

  Scenario: A cache-disabled matcher can parse without changing a read-only project
    Given AST caching is explicitly disabled
    And the AST project database is read-only
    When an AST matcher with no results runs against the project
    Then the matcher leaves the project database unchanged
    And no AST cache directory is created

  Scenario: An unwritable project cache remains optional during extraction
    Given an enabled AST cache with a filesystem-read-only project database
    When extraction rebuilds the missing AST without write access to the project
    Then extraction succeeds while reporting the unavailable cache metadata
