Feature: Stored compilation options
  A cpp-indexer file registry can replace compile_commands.json as the command source.

  Scenario: JSON compilation database remains a supported command source
    Given realistic shared-header declarations, definitions, parameters, and relations
    When the real facts-tool indexes both translation units using compile_commands.json
    Then every captured symbol uses a registered nonzero FileId

  Scenario: Stored commands reproduce JSON compilation database extraction
    Given realistic shared-header declarations, definitions, parameters, and relations
    When the real facts-tool reruns using only stored compile options
    Then the stored-option extraction matches the JSON extraction
    And every captured symbol uses a registered nonzero FileId

  Scenario: Stored commands resolve cpp-indexer component labels
    Given realistic shared-header declarations, definitions, parameters, and relations
    When the real facts-tool reruns using a stored component-label include path
    Then the stored-option extraction matches the JSON extraction
    And every captured symbol uses a registered nonzero FileId

  Scenario: JSON compilation database takes precedence over malformed stored options
    Given realistic shared-header declarations, definitions, parameters, and relations
    When the real facts-tool reruns with malformed stored options while JSON remains
    Then the facts-tool run succeeds
    And the stored-option extraction matches the JSON extraction

  Scenario: Malformed stored options are rejected without JSON fallback
    Given realistic shared-header declarations, definitions, parameters, and relations
    When the real facts-tool runs using malformed stored options without JSON
    Then the facts-tool run fails
    And the diagnostic mentions malformed compile options

  Scenario: Every requested source needs a stored command
    Given realistic shared-header declarations, definitions, parameters, and relations
    When the real facts-tool runs without a stored command for two.cpp
    Then the facts-tool run fails
    And the diagnostic mentions the missing compile command for two.cpp
