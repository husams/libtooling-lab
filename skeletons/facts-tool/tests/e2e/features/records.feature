Feature: C++ record declarations and definitions
  Structs, unions, and classes share record storage while retaining definition state.

  Background:
    Given realistic shared-header declarations, definitions, parameters, and relations
    When the real facts-tool indexes both translation units using compile_commands.json

  Scenario: Every C++ record kind uses record facts
    Then struct, union, and class declarations and definitions use record facts

  Scenario: Record definitions are distinguished from forward declarations
    Then defined records have definitions and forward-only records do not
