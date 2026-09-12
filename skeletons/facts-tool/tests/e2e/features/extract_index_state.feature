Feature: Per-file index state recorded by extract
  Extract records what it last did to each file, so a later run can skip
  sources whose recorded state is still up to date.

  Scenario: A first extract marks every extracted file
    Given a project with two freshly imported, non-git sources
    When the real facts-tool extracts every source
    Then every extracted file row records indexed=1 with a non-empty timestamp
    And every extracted file's facts_db matches the facts database path
    And every extracted file's git_commit is NULL
    And "file show" prints the index state for the first source

  Scenario: A second identical extract finds nothing to do
    Given a project with two freshly imported, non-git sources
    When the real facts-tool extracts every source
    And the real facts-tool extracts every source again
    Then the extract exits 0
    And the extract reports up to date with nothing to extract
    And the symbol count is unchanged

  Scenario: Only the source whose mtime moved is re-extracted
    Given a project with two freshly imported, non-git sources
    When the real facts-tool extracts every source
    And the first source's mtime moves into the future
    And the real facts-tool extracts every source again
    Then the extract reports 1 stale source(s) and 1 up-to-date source(s)

  Scenario: --force re-extracts every source regardless of index state
    Given a project with two freshly imported, non-git sources
    When the real facts-tool extracts every source
    And the real facts-tool force-extracts every source again
    Then the extract reports 2 stale source(s) and 0 up-to-date source(s)

  Scenario: A git-rooted project records the tracked commit
    Given a git-rooted project with one committed source
    When the real facts-tool extracts the git-rooted project
    Then the extracted file's git_commit equals the repository's HEAD
    When an unrelated file is committed to the git-rooted project
    And the real facts-tool extracts the git-rooted project again
    Then the extract reports 1 stale source(s) and 0 up-to-date source(s)
    And the extracted file's git_commit equals the repository's HEAD

  Scenario: Editing an included header re-extracts the including source
    Given a project with a source that includes a header
    When the real facts-tool extracts that source
    And only the included header is edited
    And the real facts-tool extracts that source again
    Then the extract reports 1 stale source(s) and 0 up-to-date source(s)
    And the header row's index state is updated too

  Scenario: A file extracted only into another facts database is still a recovery candidate
    Given an app that calls into a library extracted into one facts database
    When the app alone is extracted into a second facts database
    Then the library source is a recovery candidate against the second facts database

  Scenario: A file already extracted into this same facts database is not a recovery candidate
    Given an app that calls into a library extracted into one facts database
    When the app and the library are both extracted into a second facts database
    Then the library source is not a recovery candidate against the second facts database

  Scenario: A catalog mutation resets index state so a plain extract re-extracts
    Given a project with two freshly imported, non-git sources
    When the real facts-tool extracts every source
    And a compile option is set on the first source
    And the real facts-tool extracts every source again
    Then the extract reports 1 stale source(s) and 1 up-to-date source(s)

  Scenario: Re-importing unchanged compile commands keeps index state
    Given a project with two freshly imported, non-git sources
    When the real facts-tool extracts every source
    And the project is reimported with unchanged compile commands
    And the real facts-tool extracts every source again
    Then the extract reports up to date with nothing to extract

  Scenario: Re-importing a changed compile option resets only that source
    Given a project with two freshly imported, non-git sources
    When the real facts-tool extracts every source
    And the project is reimported with a changed compile option for the first source
    And the real facts-tool extracts every source again
    Then the extract reports 1 stale source(s) and 1 up-to-date source(s)

  Scenario: An old-schema project database still extracts
    Given a project with two freshly imported, non-git sources
    And the project database predates the index-state columns
    When the real facts-tool extracts every source
    Then the extract exits 0
    And every extracted file row records indexed=1 with a non-empty timestamp
