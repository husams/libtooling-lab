Feature: C++ templates
  Record, function, and method template patterns retain their ordinary symbol
  facts, while concrete instances retain supplied arguments and point back to
  the pattern that produced them.

  Background:
    Given realistic shared-header declarations, definitions, parameters, and relations
    When the real facts-tool indexes both translation units using compile_commands.json

  Scenario: Class, struct, and union templates retain their record identity and slots
    Then the template symbols include
      | qualified_name       | node | kind |
      | e2e::ClassTemplate   | 2    | 8    |
      | e2e::StructTemplate  | 2    | 7    |
      | e2e::UnionTemplate   | 2    | 11   |
    And the declared template arguments are
      | qualified_name      | position | name      | is_parameter_pack | is_non_type | is_template_template |
      | e2e::ClassTemplate  | 0        | T         | 0                 | 0           | 0                    |
      | e2e::ClassTemplate  | 1        | Container | 0                 | 0           | 1                    |
      | e2e::StructTemplate | 0        | T         | 0                 | 0           | 0                    |
      | e2e::StructTemplate | 1        | N         | 0                 | 1           | 0                    |
      | e2e::UnionTemplate  | 0        | Ts        | 1                 | 0           | 0                    |
    And every non-type template argument has a predefined type ID

  Scenario: Free-function and method templates retain function facts and slots
    Then the template symbols include
      | qualified_name                              | node | kind |
      | e2e::functionTemplate                       | 1    | 13   |
      | e2e::MethodTemplateFixture::methodTemplate  | 1    | 17   |
    And the declared template arguments are
      | qualified_name                              | position | name | is_parameter_pack | is_non_type | is_template_template |
      | e2e::functionTemplate                       | 0        | T    | 0                 | 0           | 0                    |
      | e2e::functionTemplate                       | 1        | N    | 0                 | 1           | 0                    |
      | e2e::MethodTemplateFixture::methodTemplate  | 0        | T    | 0                 | 0           | 0                    |
    And the persisted method ownership relations include
      | source                                     | destination                | kind |
      | e2e::MethodTemplateFixture::methodTemplate | e2e::MethodTemplateFixture | 9    |

  Scenario: Class, function, and method instances retain supplied arguments
    Then the supplied template parameters include
      | qualified_name                             | position | value | type        | kind | pack_index |
      | e2e::StructTemplate                        | 0        |       | e2e::Widget | 1    | -1         |
      | e2e::StructTemplate                        | 1        | 7     |             | 2    | -1         |
      | e2e::UnionTemplate                         | 0        |       | e2e::Widget | 4    | 0          |
      | e2e::UnionTemplate                         | 1        |       | e2e::Policy | 4    | 1          |
      | e2e::functionTemplate                      | 0        |       | e2e::Widget | 1    | -1         |
      | e2e::functionTemplate                      | 1        | 9     |             | 2    | -1         |
      | e2e::MethodTemplateFixture::methodTemplate | 0        |       | e2e::Widget | 1    | -1         |
    And a partial record specialization retains open slots and supplied values
    And the template instance relations include
      | source                                     | destination                                | kind |
      | e2e::StructTemplate                        | e2e::StructTemplate                        | 5    |
      | e2e::UnionTemplate                         | e2e::UnionTemplate                         | 5    |
      | e2e::functionTemplate                      | e2e::functionTemplate                      | 5    |
      | e2e::MethodTemplateFixture::methodTemplate | e2e::MethodTemplateFixture::methodTemplate | 5    |
      | e2e::StructTemplate                        | e2e::StructTemplate                        | 4    |
      | e2e::functionTemplate                      | e2e::functionTemplate                      | 4    |
      | e2e::MethodTemplateFixture::methodTemplate | e2e::MethodTemplateFixture::methodTemplate | 4    |
    And the template argument type relations include
      | source                                     | destination | position |
      | e2e::StructTemplate                        | e2e::Widget | 0        |
      | e2e::UnionTemplate                         | e2e::Widget | 0        |
      | e2e::UnionTemplate                         | e2e::Policy | 1        |
      | e2e::functionTemplate                      | e2e::Widget | 0        |
      | e2e::MethodTemplateFixture::methodTemplate | e2e::Widget | 0        |
    And the persisted instance method ownership relations include
      | source                                     | destination                | kind |
      | e2e::MethodTemplateFixture::methodTemplate | e2e::MethodTemplateFixture | 9    |
