Feature: C++ template patterns
  Record, function, and method template patterns retain their ordinary symbol
  facts and persist their declared template slots in source order.

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
