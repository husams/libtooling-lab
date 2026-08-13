Feature: Header-defined types used by source files
  A record defined in a header retains its identity when a source file derives from it or passes it by value.
  Integer boolean values are shown exactly as persisted by SQLite: 0 or 1.

  Background:
    Given realistic shared-header declarations, definitions, parameters, and relations
    When the real facts-tool indexes both translation units using compile_commands.json

  Scenario: A source-defined record inherits from a header-defined record
    Then these symbols are stored in their declaring fixture files
      | qualified_name   | fixture    |
      | e2e::MyRecord    | shared.hpp |
      | e2e::MyRecord::s | shared.hpp |
      | e2e::X           | one.cpp    |
    And the persisted direct inheritance fields include
      | source | destination   | kind | position | access | is_virtual_base | is_implicit | is_lexical | count |
      | e2e::X | e2e::MyRecord | 2    | 0        | public | 0               | 0           | 0          | 1     |

  Scenario: A source-defined function accepts a header-defined record
    Then these symbols are stored in their declaring fixture files
      | qualified_name | fixture |
      | e2e::fun       | one.cpp  |
    And the persisted parameters for e2e::fun are
      | position | name | resolved_type  | is_pointer | is_lvalue_reference | is_rvalue_reference | is_forwarding_reference | is_const | is_pack | has_default |
      | 0        | x    | e2e::MyRecord  | 0          | 0                   | 0                   | 0                       | 0        | 0       | 0           |
      | 1        | a    | predefined:int | 0          | 0                   | 0                   | 0                       | 0        | 0       | 0           |
