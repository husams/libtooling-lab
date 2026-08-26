Feature: Import handles symlinked compilation sources
  A compilation database names sources logically. A generated source that is a
  symlink into a build mirror still belongs to the project that lists it.

  Scenario: Import accepts valid commands whose sources resolve through symlinks
    Given a project with a Git root at "project"
    And "project/generated/source.cpp" is a symlink to a file outside the project root
    And a compilation database contains a valid command for "project/generated/source.cpp"
    When the real facts-tool imports the compilation database
    Then import succeeds
    And the stored active clone path is the project Git root
    And the stored repository name is not empty
    And the generated source has a registered file identity

  Scenario: Extraction attributes a symlinked source to its in-project identity
    Given a project with a Git root at "project"
    And "project/generated/source.cpp" is a symlink to a file outside the project root
    And a compilation database contains a valid command for "project/generated/source.cpp"
    When the real facts-tool imports and extracts the symlinked source
    Then extraction succeeds
    And the extracted symbols belong to the in-project file identity
