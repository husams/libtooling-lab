Feature: Canonical source file registry
  Captured symbols refer only to canonical files imported before extraction.

  Background:
    Given realistic shared-header declarations, definitions, parameters, and relations
    When the real facts-tool indexes both translation units using compile_commands.json

  Scenario: Every symbol uses a preimported file identity
    Then the file registry contains these canonical fixture paths
      | fixture   |
      | dependent_base.cpp |
      | dependent_template_types.cpp |
      | external_targets.cpp |
      | external_template_specialization.cpp |
      | references.hpp |
      | references_one.cpp |
      | references_two.cpp |
      | shared.hpp |
      | system/external_base.hpp |
      | toolchain_targets.cpp |
      | one.cpp    |
      | two.cpp    |
    And every registered FileId is greater than zero
    And every captured symbol uses a registered nonzero FileId
