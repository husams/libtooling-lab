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
      | modern/iface.cppm |
      | modern/interface.ccm |
      | modern/kernel.cu |
      | modern/kernel.cuh |
      | modern/module.mpp |
      | modern/partition.cxxm |
      | modern/unit.ixx |
      | references.hpp |
      | references_one.cpp |
      | references_two.cpp |
      | shared.hpp |
      | system/external_base.hpp |
      | system/external_string |
      | toolchain_targets.cpp |
      | one.cpp    |
      | two.cpp    |
    And every registered FileId is greater than zero
    And every captured symbol uses a registered nonzero FileId

  Scenario: Files that are not C or C++ inputs stay out of the registry
    Then the file registry excludes these fixture paths
      | fixture                      |
      | noncxx/helper.py             |
      | noncxx/requirements.txt      |
      | noncxx/LICENSE.TXT           |
      | noncxx/registry.feature      |
      | noncxx/TargetLibraryInfo.td  |
      | noncxx/module.modulemap      |
      | noncxx/.clang-format         |
      | noncxx/README                |
      | noncxx/Makefile              |
      | noncxx/LICENSE               |
    And every registered path is a C or C++ input

  Scenario: Importing an unchanged project again registers nothing new
    When the same project is imported again
    Then the import reports no newly registered files
