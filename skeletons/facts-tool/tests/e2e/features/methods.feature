Feature: C++ record methods
  Record methods retain method-specific flags and point to their owning record.
  Integer boolean values are shown exactly as persisted by SQLite: 0 or 1.

  Background:
    Given realistic shared-header declarations, definitions, parameters, and relations
    When the real facts-tool indexes both translation units using compile_commands.json

  Scenario: Methods defined inline in a class retain their details and owner
    Then the persisted method symbol fields include
      | qualified_name                       | node | is_definition | is_inline | is_virtual | is_pure | is_override | is_defaulted | is_deleted |
      | e2e::MethodFixture::inlineMethod     | 1    | 1             | 1         | 1          | 0       | 0           | 0            | 0          |
      | e2e::MethodFixture::pureMethod       | 1    | 0             | 0         | 1          | 1       | 0           | 0            | 0          |
      | e2e::MethodFixture::operator==       | 1    | 1             | 1         | 0          | 0       | 0           | 1            | 0          |
      | e2e::MethodFixture::deletedMethod    | 1    | 1             | 1         | 0          | 0       | 0           | 0            | 1          |
      | e2e::Policy::apply                   | 1    | 1             | 1         | 1          | 0       | 0           | 0            | 0          |
      | e2e::CompositeWidget::apply          | 1    | 1             | 1         | 1          | 0       | 1           | 0            | 0          |
    And the persisted method ownership relations include
      | source                               | destination        | kind |
      | e2e::MethodFixture::inlineMethod     | e2e::MethodFixture | 9    |
      | e2e::MethodFixture::pureMethod       | e2e::MethodFixture | 9    |
      | e2e::MethodFixture::operator==       | e2e::MethodFixture | 9    |
      | e2e::MethodFixture::deletedMethod    | e2e::MethodFixture | 9    |
      | e2e::Policy::apply                   | e2e::Policy        | 9    |
      | e2e::CompositeWidget::apply          | e2e::CompositeWidget | 9  |

  Scenario: A method declared with its class in a header is defined in a source file
    Then the persisted method symbol fields include
      | qualified_name                         | node | is_definition | is_inline | is_virtual | is_pure | is_override | is_defaulted | is_deleted |
      | e2e::MethodFixture::outOfLineMethod    | 1    | 1             | 0         | 0          | 0       | 0           | 0            | 0          |
    And these symbols are stored in their declaring fixture files
      | qualified_name                         | fixture    |
      | e2e::MethodFixture                     | shared.hpp |
      | e2e::MethodFixture::outOfLineMethod    | shared.hpp |
    And the persisted method ownership relations include
      | source                                  | destination        | kind |
      | e2e::MethodFixture::outOfLineMethod     | e2e::MethodFixture | 9    |
