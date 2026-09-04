Feature: Forced include headers use compiler lookup semantics
  The import path preserves compiler header search for textual forced includes
  while still resolving explicit paths into the stored project configuration.

  Scenario: StandardHeaderByName
    Given a temporary forced-include fixture from the B-028 note
    And the compiler control for the standard forced header succeeds
    When the real facts-tool imports the fixed command with the standard forced header
    Then the forced-include import succeeds
    And the stored forced-include options preserve compiler order
    When the real facts-tool imports the compilation database with the standard forced header
    Then the forced-include import succeeds
    And the stored forced-include options preserve compiler order

  Scenario: StoredForcedIncludeExtraction
    Given a temporary forced-include fixture from the B-028 note
    When the real facts-tool imports the compilation database with the standard forced header
    And the real facts-tool repeats that import with fresh temporary storage
    And the real facts-tool extracts using the stored forced header
    Then the forced-include import succeeds
    And the stored forced-include options preserve compiler order

  Scenario: ExplicitForcedHeaderPaths
    Given a temporary forced-include fixture with an explicit local header
    When the real facts-tool imports the relative forced header path
    Then the forced-include import succeeds
    And the stored forced-include option is "include/forced.hpp"
    And extracting with the stored forced header succeeds
    When the real facts-tool imports the absolute forced header path
    Then the forced-include import succeeds
    And the stored forced-include option is the unchanged absolute header path
    And extracting with the stored forced header succeeds

  Scenario: ImacrosHeaderPath
    Given a temporary forced-include fixture with an explicit local header
    When the real facts-tool imports the imacros forced header path
    Then the forced-include import succeeds
    And the stored imacros option is unchanged
    And extracting with the stored forced header succeeds

  Scenario: MissingForcedHeader
    Given a temporary forced-include fixture from the B-028 note
    When the real facts-tool imports a missing forced header
    Then the forced-include import fails with an actionable diagnostic
