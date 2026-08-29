Feature: Extraction completeness for undeclared instances and un-USR-able declarations
  Naming a class template specialization without requiring it to be complete, and
  declaring an entity Clang refuses to give a USR, must both leave extraction
  complete: the unit is committed, provenance is never fabricated, and no
  identity is synthesized.

  Scenario: Undeclared specializations do not abort extraction
    Given a compile database for the undeclared-template fixture
    When the real extraction command indexes the undeclared-template fixture
    Then the undeclared-template extraction exits successfully without incomplete diagnostics
    And no template_instance relation failure is reported

  Scenario: Undeclared specializations do not discard the unit
    Given a compile database for the undeclared-template fixture
    When the real extraction command indexes the undeclared-template fixture
    Then the undeclared-template canary and owners are committed

  Scenario: Undeclared specializations record no provenance
    Given a compile database for the undeclared-template fixture
    When the real extraction command indexes the undeclared-template fixture
    Then each undeclared specialization carries no specializes and no instantiates relation
    And the undeclared record-argument specialization still carries its template argument type edge

  Scenario: Genuine instantiations keep their provenance
    Given a compile database for the undeclared-template fixture
    When the real extraction command indexes the undeclared-template fixture
    Then the implicitly instantiated specialization points to the primary template
    And the implicitly instantiated specialization carries its template argument type edge

  Scenario: Un-USR-able declarations are skipped, not failed
    Given a compile database for the invalid-usr fixture
    When the real extraction command indexes the invalid-usr fixture
    Then the invalid-usr extraction exits successfully without incomplete diagnostics
    And no symbol row exists for the un-USR-able declaration
    And no relation references the un-USR-able declaration
    And no identity was synthesized for the un-USR-able declaration

  Scenario: Un-USR-able declarations are traced and siblings survive
    Given a compile database for the invalid-usr fixture
    When the real extraction command indexes the invalid-usr fixture at verbosity 3
    Then the trace records the skipped declaration with reason invalid USR
    And every named sibling declaration in the same record is committed

  Scenario: Template relation persistence failures identify the exact edge
    Given a compile database for the undeclared-template fixture
    When template argument relation persistence is forced to fail on a rerun
    Then the template relation insertion exits unsuccessfully
    And the template relation diagnostic names its kind, SymbolIds, and position

  Scenario Outline: Extraction is deterministic and referentially intact
    Given a compile database for the <fixture> fixture
    When the real extraction command indexes the <fixture> fixture twice
    Then both runs produce identical symbol identities
    And the output database passes PRAGMA foreign_key_check

    Examples:
      | fixture             |
      | undeclared-template |
      | invalid-usr         |
