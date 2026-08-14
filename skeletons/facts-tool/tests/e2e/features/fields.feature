Feature: C++ record fields
  Record fields retain symbol details and point to their owning record.
  Integer boolean values are shown exactly as persisted by SQLite: 0 or 1.

  Background:
    Given realistic shared-header declarations, definitions, parameters, and relations
    When the real facts-tool indexes both translation units using compile_commands.json

  Scenario: Fields declared in a shared header retain their details and owner
    Then the persisted field symbol fields include
      | qualified_name         | node | is_definition | access |
      | e2e::MyRecord::s       | 4    | 1             | public |
      | e2e::Payload::integral | 4    | 1             | public |
    And these symbols are stored in their declaring fixture files
      | qualified_name         | fixture    |
      | e2e::MyRecord::s       | shared.hpp |
      | e2e::Payload::integral | shared.hpp |
    And the persisted field ownership relations include
      | source                 | destination   | kind |
      | e2e::MyRecord::s       | e2e::MyRecord | 8    |
      | e2e::Payload::integral | e2e::Payload  | 8    |

  Scenario: Struct and class fields retain their access and owner
    Then the persisted field symbol fields include
      | qualified_name         | node | is_definition | access |
      | e2e::Widget::value     | 4    | 1             | public |
      | e2e::Policy::multiplier | 4   | 1             | public |
    And the persisted field ownership relations include
      | source                  | destination | kind |
      | e2e::Widget::value      | e2e::Widget | 8    |
      | e2e::Policy::multiplier | e2e::Policy | 8    |
