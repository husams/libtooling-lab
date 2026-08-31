Feature: Manage files, clones, and extracted symbols
  The commands use real imported databases and preserve checkout contents.

  Background:
    Given an imported catalog with two repositories and independent components

  Scenario: Add and inspect a manual file under the deepest indexed directory
    Given a manual source inside the deepest indexed directory
    When I run the catalog command "file add {manual-file} --driver {compiler} --arg=-DVALUE=1 --arg=-I --arg=include"
    Then the catalog command succeeds
    And the manual file is stored under the deepest directory with exact compilation details
    When I run the catalog command "file show {manual-file}"
    Then the catalog command succeeds
    And the catalog output contains "ARGS OVERRIDDEN: true"
    When I run the catalog command "file ls"
    Then the catalog command succeeds
    And the catalog output contains "manual.cpp"

  Scenario: An explicit working directory is canonicalized
    Given a manual source inside the deepest indexed directory
    When I run the catalog command "file add {manual-file} --driver {compiler} --working-directory {working-directory}"
    Then the catalog command succeeds
    And the manual file uses the explicit working directory

  Scenario: Add a file beneath an unregistered child directory without collapsing its path
    Given a source beneath an unregistered child directory with an existing basename
    When I run the catalog command "file add {nested-file} --driver {compiler}"
    Then the catalog command succeeds
    And the nested file has its own registered directory and round-trips exactly
    When I run the catalog command "file show {nested-file}"
    Then the catalog command succeeds
    And the nested file appears at its exact path
    When I run the catalog command "file list"
    Then the catalog command succeeds
    And the nested file appears at its exact path

  Scenario Outline: Invalid file additions are atomic
    Given a manual source inside the deepest indexed directory
    And an existing source outside every indexed directory
    When I run the catalog command "<command>"
    Then the catalog command fails with "<diagnostic>"
    And the entire catalog is unchanged

    Examples:
      | command                                                                      | diagnostic                                      |
      | file add {outside-file} --driver {compiler}                                  | outside every registered indexed directory      |
      | file add {manual-file} --driver {missing-path}                               | compiler driver does not exist or is invalid     |
      | file add {manual-file} --driver {compiler} --working-directory {missing-path} | directory not found                              |

  Scenario: Adding a duplicate file is atomic
    Given a registered manual source
    When I run the catalog command "file add {manual-file} --driver {compiler}"
    Then the catalog command fails with "file already registered"
    And the entire catalog is unchanged

  Scenario Outline: Removing a file changes only its catalog row
    Given a registered manual source
    When I run the catalog command "file <verb> {manual-file}"
    Then the catalog command succeeds
    And the manual file row is absent but its source remains

    Examples:
      | verb   |
      | rm     |
      | remove |

  Scenario Outline: File list aliases document an empty catalog
    Given the catalog contains no file rows
    When I run the catalog command "file <verb>"
    Then the catalog command succeeds
    And the catalog output contains "No files registered"

    Examples:
      | verb |
      | list |
      | ls   |

  Scenario: File show rejects an unknown path
    When I run the catalog command "file show {missing-path}"
    Then the catalog command fails with "not found"
    And the entire catalog is unchanged

  Scenario: Set-option replaces every old sequence with exactly one new sequence on matches
    Given repeated compile options on matching and nonmatching files
    When I run the catalog command "file set-option --match=src/.+\\.cpp$ --arg=-I --arg=include"
    Then the catalog command succeeds
    And matching files contain the exact option sequence once and nonmatching files are unchanged

  Scenario: Clear-option removes every exact contiguous occurrence only from matches
    Given repeated compile options on matching and nonmatching files
    When I run the catalog command "file clear-option --match=^src/ --arg=-I --arg=include"
    Then the catalog command succeeds
    And matching files contain no exact option sequence and nonmatching files are unchanged

  Scenario Outline: Invalid or unmatched option edits are atomic
    Given repeated compile options on matching and nonmatching files
    When I run the catalog command "file set-option --match=<pattern> --arg=-DNEW"
    Then the catalog command fails with "<diagnostic>"
    And the entire catalog is unchanged

    Examples:
      | pattern       | diagnostic                                          |
      | [             | invalid regular expression                          |
      | never-matches | regular expression matched no registered files     |

  Scenario: Import omitting a manually overridden file preserves all of its fields
    Given a registered manual source with overridden compilation options
    When I reimport commands that omit the manual source
    Then the manual file and every compilation field are preserved

  Scenario: Import containing a manually overridden file replaces its command and clears the override
    Given a registered manual source with overridden compilation options
    When I reimport commands that include the manual source
    Then the manual file command is replaced and its override is cleared

  Scenario Outline: Remove-clone aliases remove only a non-active registration
    Given a registered second clone of the demo repository
    When I run the catalog command "repo <verb> demo second"
    Then the catalog command succeeds
    And the second clone registration is absent while both checkouts remain
    And the logical component directory and file rows are unchanged

    Examples:
      | verb         |
      | rm-clone     |
      | remove-clone |

  Scenario: Removing the active clone is refused atomically
    When I run the catalog command "repo rm-clone demo original"
    Then the catalog command fails with "cannot remove active clone"
    And the entire catalog is unchanged

  Scenario Outline: Symbol list aliases read only the facts database
    When I run the symbol command "<verb>"
    Then the symbol command succeeds
    And symbol output lists the extracted catalog function with aligned columns

    Examples:
      | verb |
      | list |
      | ls   |

  Scenario: Symbol show renders human-readable function metadata
    When I show the extracted catalog function
    Then the symbol command succeeds
    And symbol output contains human-readable function metadata

  Scenario: Symbol show renders human-readable record metadata without catalog configuration
    When I show the extracted catalog record without catalog configuration
    Then the symbol command succeeds
    And symbol output contains human-readable record metadata

  Scenario: Symbol show rejects an unknown qualified name
    When I run the symbol command "show definitely::missing"
    Then the symbol command fails with "not found"

  Scenario: Symbol show rejects an empty qualified name
    When I run the symbol command "show ''"
    Then the symbol command fails with "not found"

  Scenario: Symbol list documents an empty facts database
    Given the facts database contains no symbols
    When I run the symbol command "list"
    Then the symbol command succeeds
    And the symbol output contains "No symbols"
