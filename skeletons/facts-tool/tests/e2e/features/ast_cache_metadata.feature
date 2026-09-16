@ast_cache
Feature: Project database owns persistent dependency and AST metadata
  Import caches preprocessing inputs and includes independently from AST files.
  Git revisions control freshness, and AST generations cannot be renewed by import.

  Background:
    Given an isolated AST cache project
    And AST caching is enabled

  Scenario: Cold import persists normalized metadata without producing an AST
    When the AST cache project runs "import"
    Then import stores normalized input, include, and Git revision rows
    And import creates neither serialized ASTs nor JSON sidecars

  Scenario: Import collects metadata during its single preprocessing pass
    Given the committed source reports each preprocessing pass
    When the AST cache project runs "import"
    Then import stores normalized input, include, and Git revision rows
    And import preprocesses the source exactly once while collecting metadata
    And import creates neither serialized ASTs nor JSON sidecars
    When the AST cache project runs "import"
    Then the unchanged dependency metadata is reused without preprocessing
    And reimport emits no preprocessing probe warning

  Scenario: Reimporting the same commit reuses metadata without duplicate rows
    Given dependency metadata has been imported into the project database
    When the AST cache project runs "import"
    Then the unchanged dependency metadata is reused without preprocessing
    And import creates neither serialized ASTs nor JSON sidecars

  Scenario: Same-commit source edits do not trigger preprocessing or refresh metadata
    Given dependency metadata has been imported into the project database
    When the source gains an uncommitted preprocessing error and is reimported
    Then the unchanged dependency metadata is reused without preprocessing

  Scenario Outline: Dependency analysis only needs database include metadata
    Given a persisted AST from extraction
    And the serialized AST is "<condition>" but database metadata remains intact
    When the AST cache project runs "dependency"
    Then dependency analysis uses database includes without loading or repairing the AST

    Examples:
      | condition |
      | missing   |
      | corrupt   |

  Scenario: Reimporting a new commit cannot make the previous AST current
    Given a persisted AST from extraction
    When a new source symbol is committed and the project is reimported
    Then the refreshed dependency snapshot does not validate the previous AST
    When the AST cache project runs "extract"
    Then extraction regenerates the AST with the newly committed symbol

  Scenario: Warm extraction ignores malformed legacy JSON sidecars
    Given a persisted AST from extraction
    And malformed legacy JSON sidecars accompany the persisted AST
    When the AST cache project runs "extract"
    Then the cached AST is reused and legacy JSON files remain untouched

  Scenario: Disabled caching does not persist dependency metadata
    Given AST caching is explicitly disabled
    When the AST cache project runs "import"
    Then the project database contains no persistent cache rows

  Scenario: Sources outside Git do not produce persistent metadata
    Given the AST cache project has no Git repository
    When the AST cache project runs "import"
    Then the project database contains no persistent cache rows

  Scenario Outline: Legacy project databases migrate without changing file identities
    Given the imported project database has legacy schema version <version>
    When the AST cache project runs "import"
    Then the project schema is migrated while file identities and compiler commands are preserved
    And import creates neither serialized ASTs nor JSON sidecars

    Examples:
      | version |
      | 0       |
      | 1       |

  Scenario: An invalid database artifact digest cannot authorize cached AST reuse
    Given a persisted AST from extraction
    And the database artifact digest is damaged
    When the AST cache project runs "extract"
    Then extraction replaces the artifact with a valid database digest

  Scenario: Relative imported commands and stored commands share one dependency snapshot
    Given dependency metadata is imported from a relative source and joined relative include option
    When the AST cache project runs "dependency"
    Then the configured cache is reused by the command
    And relative and stored command spellings retain the single imported dependency snapshot
    When the AST cache project runs "extract"
    Then the cold AST is persisted
    And relative and stored command spellings retain the single imported dependency snapshot
    And the relative include header is represented by the extracted facts
