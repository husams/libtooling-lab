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
      | b027_std_string_external_callee.cpp |
      | b027_stream_temporary_external_callee.cpp |
      | call_graph.hpp |
      | call_graph_one.cpp |
      | call_graph_possible.cpp |
      | call_graph_template.cpp |
      | call_graph_two.cpp |
      | external_targets.cpp |
      | external_template_specialization.cpp |
      | forced-include/include/forced.hpp |
      | forced-include/optional.cpp |
      | forced-include/paths.cpp |
      | forced-include/system/forced.hpp |
      | forward_template_target.cpp |
      | forward_template_target_system.hpp |
      | initializer_dependent_alignment.cpp |
      | invalid_usr_declarations.cpp |
      | modern/iface.cppm |
      | modern/interface.ccm |
      | modern/kernel.cu |
      | modern/kernel.cuh |
      | modern/module.mpp |
      | modern/partition.cxxm |
      | modern/unit.ixx |
      | qualifiers/cv.hpp |
      | qualifiers/implicit.hpp |
      | qualifiers/one.cpp |
      | qualifiers/specifiers.hpp |
      | qualifiers/two.cpp |
      | references.hpp |
      | references_one.cpp |
      | references_two.cpp |
      | relation_resolution.cpp |
      | relation_resolution.hpp |
      | return_types.cpp                     |
      | shared.hpp |
      | system/external_base.hpp |
      | system/external_string |
      | targeted_match.cpp |
      | targeted_match_broken.cpp |
      | targeted_match_two.cpp |
      | toolchain_targets.cpp |
      | undeclared_template_instances.cpp |
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
