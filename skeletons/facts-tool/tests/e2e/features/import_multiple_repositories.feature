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

  Scenario Outline: Generated imports reuse an existing catalog without ownership metadata
    Given two Git repositories share one generated project database
    And the shared catalog already exists from "<creation>"
    When I import both repositories using generated selection starting with "<checkout>"
    Then both repositories retain their sources headers and compilation commands
    And the existing catalog keeps its identities and indexed state
    When I extract both repository sources using their stored commands
    Then both repository sources are indexed
    When I reimport both repositories in reverse order twice
    Then both repositories retain their sources headers and compilation commands
    And every repository clone component directory and file keeps its identity
    And neither repository loses its unchanged indexed state
    When I export the stored compilation commands from each repository
    Then each export contains only that repository's original compilation command

    Examples:
      | creation                         | checkout |
      | explicit import                  | same     |
      | explicit import                  | other    |
      | environment import               | other    |
      | explicit repository registration | other    |

  Scenario Outline: Imports remove obsolete owner metadata without replacing the catalog
    Given two Git repositories share one generated project database
    And the shared catalog already exists from "explicit import"
    And the shared catalog has an obsolete owner marker for an unrelated checkout
    When I import both repositories using "<selection>" database selection
    Then both repositories retain their sources headers and compilation commands
    And the existing catalog keeps its identities and indexed state

    Examples:
      | selection   |
      | generated   |
      | explicit    |
      | environment |

  Scenario: Shared configuration follows each repository's active clone without an owner
    Given two Git repositories share one generated project database
    When I import each repository from its own checkout
    And I register a second clone of each shared repository
    Then each repository has two clones and one active local checkout
    When I switch to both second clones and import from those checkouts
    Then the shared catalog resolves unchanged file identities through the active clones
    When I switch back and reimport both original checkouts
    Then each repository has two clones and one active local checkout
    And all repository clone and file identities survive both switches
