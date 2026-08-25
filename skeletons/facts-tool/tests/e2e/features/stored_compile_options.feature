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

  Scenario: Explicit refresh replaces malformed stored options from JSON
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

  Scenario: Project configuration replaces the ambiguous file output option
    Given realistic shared-header declarations, definitions, parameters, and relations
    When the real facts-tool is invoked with the deprecated --files-out option
    Then the facts-tool run fails
    And the diagnostic reports --files-out as an unknown option

  Scenario: An unrelated command's missing include directory does not block extraction
    Given realistic shared-header declarations, definitions, parameters, and relations
    When the real facts-tool imports and extracts one.cpp while two.cpp keeps a missing include directory
    Then the facts-tool run succeeds
    And no diagnostic reports a file registration failure

  Scenario: A missing include directory on the selected command is skipped with a diagnostic
    Given realistic shared-header declarations, definitions, parameters, and relations
    When the real facts-tool imports and extracts one.cpp with a missing include directory
    Then the facts-tool run succeeds
    And the import diagnostic reports the skipped include directory
