Feature: Persist callable return types
  Background:
    Given the isolated C++17 callable return-type fixture
    When I import and extract the callable fixture through the real CLI

  Scenario Outline: Persist and query the return type of <selector>
    Then callable "<selector>" has exactly one return edge and canonical type "<type>"
    And the callable return type is displayed by the supported symbol query

    Examples:
      | selector        | type                        |
      | free_function   | int                         |
      | free_void       | void                        |
      | free_value      | return_types::Widget        |
      | free_pointer    | return_types::Widget *      |
      | free_reference  | const return_types::Widget &|
      | trailing_return | return_types::Widget        |
      | method          | const return_types::Widget &|
      | static_method   | int                         |
      | object_value    | return_types::Widget        |
      | object_int      | int                         |
      | explicit_lambda | return_types::Widget *      |
      | deduced_lambda  | int                         |

  Scenario: Exact return-edge inventory survives repeated extraction
    Then every selected callable has exactly its expected return-type facts
    When I extract the callable fixture again into the same database
    Then callable identities and return-type facts are unchanged without duplicates

  Scenario: Upgrade an existing facts database without changing existing facts
    Given the callable facts database has the previous schema without return types
    Then legacy callable symbols remain queryable without rewriting the database
    When I extract the callable fixture again into the same database
    Then every selected callable has exactly its expected return-type facts
    And the upgrade preserves existing identities and unrelated facts
