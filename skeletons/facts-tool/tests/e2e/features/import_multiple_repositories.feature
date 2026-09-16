Feature: Import multiple repositories into one project database
  A shared configuration can select one project database for independent Git
  repositories. Importing or refreshing either repository must preserve the
  other repository's identities and compilation commands.

  Scenario Outline: Sequential repository imports preserve both catalogs
    Given two Git repositories share one generated project database
    And the shared database is selected by "<selection>"
    And the shared repositories are "<registration>"
    When I import each repository from its own checkout
    Then both repositories retain their sources headers and compilation commands
    When I extract both repository sources using their stored commands
    Then both repository sources are indexed
    When I reimport both repositories in reverse order twice
    Then both repositories retain their sources headers and compilation commands
    And every repository clone component directory and file keeps its identity
    And neither repository loses its unchanged indexed state
    When I export the stored compilation commands from each repository
    Then each export contains only that repository's original compilation command

    Examples:
      | registration      | selection |
      | discovered        | generated |
      | registered first  | generated |
      | discovered        | explicit  |
