Feature: Dependent function-template parameter types
  Primary function templates and their concrete instances retain resolvable
  parameter types without losing specialization provenance.

  Scenario: Extract dependent template parameter types successfully
    Given a reproducing compile database for dependent template parameter types
    When the real extraction command indexes the dependent-template fixture
    Then the dependent-template extraction exits successfully without incomplete diagnostics
    And dependent template parameters retain resolvable types and modifiers
    And dependent function-template instances point to their primary templates

  Scenario: Extract dependent decltype aliases and parameter packs successfully
    Given a reproducing compile database for dependent template parameter types
    When the real extraction command indexes the dependent-template fixture
    Then the dependent-template extraction exits successfully without incomplete diagnostics
    And the dependent decltype alias is captured
    And dependent parameter packs retain resolvable types and modifiers

  Scenario: Extract deduced and parenthesized function types successfully
    Given a reproducing compile database for dependent template parameter types
    When the real extraction command indexes the dependent-template fixture
    Then the dependent-template extraction exits successfully without incomplete diagnostics
    And deduced variables and parenthesized function types are captured
