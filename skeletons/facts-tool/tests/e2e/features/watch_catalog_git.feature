Feature: Each monitored clone honors Git ignore rules

  Scenario: Root and nested ignore rules exclude untracked sources from all automatic work
    Given an indexed Git repository with nested ignore rules and tracked exceptions
    When I edit the excluded sources and then an allowed source
    Then excluded sources are neither reimported nor reindexed

  Scenario Outline: Negation and tracked files remain eligible for monitoring
    Given an indexed Git repository with nested ignore rules and tracked exceptions
    When I edit the Git exception "<kind>"
    Then the Git exception is automatically indexed

    Examples:
      | kind            |
      | negated pattern |
      | tracked file    |

  Scenario: Removing an ignore rule takes effect without restarting
    Given an indexed Git repository with nested ignore rules and tracked exceptions
    When I remove an ignore rule while the server is running
    Then the newly allowed source is monitored without a restart

  Scenario: Adding an ignore rule takes effect without restarting
    Given an indexed Git repository with nested ignore rules and tracked exceptions
    When I add an ignore rule while the server is running
    And I edit the excluded sources and then an allowed source
    Then excluded sources are neither reimported nor reindexed

  Scenario: Different repositories apply their own Git ignore files
    Given each repository has a different ignore policy for the same filename
    When I edit the matching filenames in both clones
    Then only the clone whose Git policy allows that filename is refreshed

  Scenario: Explicit YAML exclusions also apply to Git-tracked sources
    Given an indexed Git repository whose tracked source is excluded in YAML
    When I edit the excluded sources and then an allowed source
    Then excluded sources are neither reimported nor reindexed

  Scenario: Plain source directories honor ignore files without acquiring Git metadata
    Given a registered plain source directory with a Git ignore file
    When I edit the excluded sources and then an allowed source
    Then excluded sources are neither reimported nor reindexed
    And monitoring has not created Git metadata in the source directory
