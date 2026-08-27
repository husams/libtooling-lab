Feature: Facts and file storage boundaries
  The file registry remains independent from extracted semantic facts.

  Background:
    Given realistic shared-header declarations, definitions, parameters, and relations
    When the real facts-tool indexes both translation units using compile_commands.json

  Scenario: File identity and captured facts use separate databases
    Then the facts database contains these tables
      | table            |
      | symbol           |
      | symbol_allocator |
      | definition       |
      | include_dependency |
      | parameter        |
      | relation         |
    And the facts database excludes the file table
    And no facts table stores opaque packed flags
    And the files database contains only these tables
      | table             |
      | clone             |
      | component         |
      | directory         |
      | file              |
      | project_registry  |
      | repository        |
      | semantic_universe |
    And the facts and files databases use different paths

  Scenario: Extraction uses a read-only project configuration
    Given a project configuration imported from a compilation database
    And the project configuration database is read-only
    When the real facts-tool extracts one translation unit
    Then extraction succeeds
    And the facts database contains the extracted symbols
    And the project configuration database is unchanged

  Scenario: An outdated project configuration is reported, not crashed on
    Given a project configuration imported from a compilation database
    And the project configuration uses an outdated file registry
    When the real facts-tool extracts one translation unit for the registry check
    Then extraction fails with an outdated-registry diagnostic

  Scenario: Extraction refuses an incomplete imported file registry
    Given a current project configuration whose registry lacks an included header
    When the real facts-tool extracts one translation unit for the registry check
    Then extraction reports the incomplete project configuration once
    And extraction emits no per-symbol incomplete diagnostics
    And extraction commits zero symbols

  Scenario: Extraction refuses a registry no import has completed
    Given a project configuration whose registry no import has completed
    When the real facts-tool extracts one translation unit for the registry check
    Then extraction reports the incomplete project configuration once
    And extraction preprocesses no translation unit
    And extraction commits zero symbols

  Scenario: Import records the registry it completed
    Given a project configuration imported from a compilation database
    Then the project configuration records a completed registry

  Scenario: Import refuses to report success when a translation unit cannot be preprocessed
    Given a compilation database whose translation unit includes a missing header
    When the real facts-tool imports that project
    Then import fails with an incomplete-registry diagnostic

  Scenario: Import refuses a prefix header that has not been compiled yet
    Given a compile command that includes a precompiled header
    When the real facts-tool imports before the prefix header is compiled
    Then import fails with an incomplete-registry diagnostic

  Scenario: A compiled prefix header supplies the identities extraction resolves
    Given a compile command that includes a precompiled header
    When the real facts-tool imports and extracts with the prefix header compiled
    Then extraction succeeds
    And the facts database contains the precompiled-header declarations
