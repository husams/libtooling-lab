@ast_cache
Feature: Project database owns persistent dependency and AST metadata
  Import prepares serialized ASTs and normalized dependency metadata together.
  Git revisions control freshness, and later consumers reuse the imported generation.

  Background:
    Given an isolated AST cache project
    And AST caching is enabled

  Scenario: Cold import persists normalized metadata and a serialized AST
    When the AST cache project runs "import"
    Then import stores normalized input, include, and Git revision rows
    And import creates a serialized AST with matching database metadata and no JSON sidecars

  Scenario: Import collects metadata and builds the AST in a single compiler pass
    Given the committed source reports each preprocessing pass
    When the AST cache project runs "import"
    Then import stores normalized input, include, and Git revision rows
    And import preprocesses the source exactly once while collecting metadata
    And import creates a serialized AST with matching database metadata and no JSON sidecars
    When the AST cache project runs "import"
    Then the unchanged dependency metadata is reused without preprocessing
    And reimport emits no preprocessing probe warning

  Scenario: Reimporting the same commit reuses metadata and AST without duplicate rows
    Given dependency metadata has been imported into the project database
    When the AST cache project runs "import"
    Then the unchanged dependency metadata is reused without preprocessing
    And import creates a serialized AST with matching database metadata and no JSON sidecars

  Scenario: Same-commit source edits do not trigger preprocessing or refresh metadata
    Given dependency metadata has been imported into the project database
    When the source gains an uncommitted preprocessing error and is reimported
    Then the unchanged dependency metadata is reused without preprocessing

  Scenario Outline: The first consumer uses the imported cache without preprocessing dirty inputs
    Given the committed source reports each preprocessing pass
    And dependency metadata has been imported into the project database
    When the uncommitted "<input>" becomes invalid and "<family>" runs
    Then the first "<family>" consumer reuses the imported cache without rescanning or rewriting it

    Examples:
      | input  | family        |
      | source | extract       |
      | header | extract       |
      | source | match         |
      | header | match         |
      | source | dependency    |
      | header | dependency    |
      | source | variable-flow |
      | header | variable-flow |

  Scenario Outline: Reimport repairs an unusable AST using existing dependency metadata
    Given the committed source reports each preprocessing pass
    And dependency metadata has been imported into the project database
    And the serialized AST is "<condition>" but database metadata remains intact
    When the AST cache project runs "import"
    Then import repairs the serialized AST without a separate dependency preprocessing pass
    When the AST cache project runs "extract"
    Then the persisted AST is reused

    Examples:
      | condition |
      | missing   |
      | corrupt   |

  Scenario Outline: Dependency analysis only needs database include metadata
    Given a persisted AST from extraction
    And the serialized AST is "<condition>" but database metadata remains intact
    When the AST cache project runs "dependency"
    Then dependency analysis uses database includes without loading or repairing the AST

    Examples:
      | condition |
      | missing   |
      | corrupt   |

  Scenario: Reimporting a new commit prepares the replacement AST before extraction
    Given a persisted AST from extraction
    When a new source symbol is committed and the project is reimported
    Then import replaces the previous AST with the refreshed dependency generation
    When the AST cache project runs "extract"
    Then extraction reuses the imported AST with the newly committed symbol

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

  Scenario: Semantic errors keep import usable without publishing an invalid AST
    Given the committed source preprocesses successfully but has a C++ semantic error
    When the AST cache project runs "import"
    Then import preserves dependency metadata without publishing an invalid AST
    When the AST cache project runs "extract"
    Then extraction reports the semantic error without a cache hit

  Scenario: An unavailable AST directory still preserves imported dependency metadata
    Given the configured cache directory is blocked by a regular file
    And the committed source reports each preprocessing pass
    When the AST cache project runs "import"
    Then import preprocesses the source exactly once while collecting metadata
    And import preserves dependency metadata without publishing an invalid AST
    And the unavailable AST directory retains its original contents
    When the AST cache project runs "dependency"
    Then the configured cache is reused by the command
    And the cached "dependency" result is complete
    And reimport emits no preprocessing probe warning

  Scenario Outline: Legacy project databases migrate without changing file identities
    Given the imported project database has legacy schema version <version>
    When the AST cache project runs "import"
    Then the project schema is migrated while file identities and compiler commands are preserved
    And import creates a serialized AST with matching database metadata and no JSON sidecars

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
    Then the import-prepared AST is reused for the first extraction
    And relative and stored command spellings retain the single imported dependency snapshot
    And the relative include header is represented by the extracted facts
