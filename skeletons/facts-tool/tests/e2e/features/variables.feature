Feature: C++ global and field initializers
  Written initializer expressions are retained, with typed evaluated values
  when Clang can fold them.

  Background:
    Given realistic shared-header declarations, definitions, parameters, and relations
    When the real facts-tool indexes both translation units using compile_commands.json

  Scenario: Initializer expressions and scalar values are queryable
    Then the persisted variable initializers include
      | qualified_name                    | expression    | evaluated_kind | evaluated_value |
      | e2e::sharedCounter                | 3             | integer        | 3               |
      | e2e::mergedGlobal                 | 11            | integer        | 11              |
      | e2e::internalGlobal               | 13            | integer        | 13              |
      | e2e::inlineGlobal                 | 14            | integer        | 14              |
      | e2e::constinitGlobal              | 15            | integer        | 15              |
      | e2e::InitializerFixture::count    | 2 + 3         | integer        | 5               |
      | e2e::InitializerFixture::enabled  | true          | boolean        | true            |
      | e2e::InitializerFixture::label    | "ready"       | string         | ready           |
      | e2e::InitializerFixture::values   | {1, 2, 3}     | none           |                 |
      | e2e::InitializerFixture::limit    | 6 * 7         | integer        | 42              |
      | e2e::InitializerFixture::name     | "static"      | string         | static          |
      | e2e::constructedWidget            | Widget{7}     | none           |                 |
      | e2e::constructedClass             | ConstClass(7) | none           |                 |
      | e2e::globalValues                 | {4, 5, 6}     | none           |                 |

  Scenario: Static data members retain flags, ownership, and type relations
    Then the persisted variable symbol fields include
      | qualified_name                 | node | is_definition | is_static | is_const | is_inline | has_internal_linkage | has_extern_storage | constant_evaluation |
      | e2e::InitializerFixture::limit | 4    | 1             | 1         | 1        | 1         | 0                    | 0                  | constexpr           |
      | e2e::InitializerFixture::name  | 4    | 1             | 1         | 0        | 0         | 0                    | 0                  | none                |
      | e2e::mergedGlobal              | 4    | 1             | 0         | 0        | 0         | 0                    | 1                  | none                |
      | e2e::internalGlobal            | 4    | 1             | 1         | 0        | 0         | 1                    | 0                  | none                |
      | e2e::inlineGlobal              | 4    | 1             | 0         | 1        | 1         | 0                    | 0                  | constexpr           |
      | e2e::constinitGlobal           | 4    | 1             | 0         | 0        | 0         | 0                    | 0                  | constinit            |
    And the persisted value relations include
      | source                         | destination             | kind |
      | e2e::InitializerFixture::limit | e2e::InitializerFixture | 8    |
      | e2e::InitializerFixture::name  | e2e::InitializerFixture | 8    |
      | e2e::constructedWidget         | e2e::Widget             | 20   |
      | e2e::constructedClass          | e2e::ConstClass         | 20   |
    And block-scope variables are not persisted
      | qualified_name           |
      | localValue               |
      | localStatic              |
