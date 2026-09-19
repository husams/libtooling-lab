Feature: Import source commands incrementally within one repository
  Importing a source updates its own compiler command while previously imported
  sources and their dependencies keep their identities and usable commands.

  Scenario Outline: Sources imported one at a time preserve existing commands
    Given an incremental import project using "<mode>" with AST caching "<cache>"
    When I import the first incremental source and extract it
    And I import the second incremental source
    Then both incremental sources retain their compiler commands and dependencies
    And the first incremental source keeps its indexed state
    When I extract both incremental sources using only stored commands
    Then both incremental sources are indexed
    When I change the first source compiler options and reimport it twice
    Then only the first incremental command changes without duplicate identities
    And the second incremental source keeps its indexed state
    When I extract both incremental sources using only stored commands
    Then both incremental sources are indexed

    Examples:
      | mode                | cache    |
      | filtered JSON       | disabled |
      | filtered JSON       | enabled  |
      | single-command JSON | disabled |
      | single-command JSON | enabled  |
      | fixed arguments     | disabled |
      | fixed arguments     | enabled  |

  Scenario: A later compilation database preserves commands absent from its input
    Given an incremental import project using "filtered JSON" with AST caching "disabled"
    When I import the complete incremental compilation database and extract its sources
    And I import a replacement compilation database containing only the changed first source
    Then only the first incremental command changes without duplicate identities
    And the second incremental source keeps its indexed state
    When I extract both incremental sources using only stored commands
    Then both incremental sources are indexed
