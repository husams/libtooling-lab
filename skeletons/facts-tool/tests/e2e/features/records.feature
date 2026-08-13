Feature: C++ record declarations and definitions
  Structs, unions, and classes share record storage while retaining definition state.

  Scenario: Every C++ record kind uses record facts
    Given realistic shared-header declarations, definitions, parameters, and relations
    When the real facts-tool indexes both translation units using compile_commands.json
    Then the record symbols are
      | qualified_name       | node |
      | e2e::CompositeWidget | 2    |
      | e2e::Deferred        | 2    |
      | e2e::Payload         | 2    |
      | e2e::Policy          | 2    |
      | e2e::PrivateWidget   | 2    |
      | e2e::PublicWidget    | 2    |
      | e2e::Widget          | 2    |

  Scenario: Record definitions are distinguished from forward declarations
    Given realistic shared-header declarations, definitions, parameters, and relations
    When the real facts-tool indexes both translation units using compile_commands.json
    Then the record definition states are
      | qualified_name       | defined |
      | e2e::CompositeWidget | yes     |
      | e2e::Deferred        | no      |
      | e2e::Payload         | yes     |
      | e2e::Policy          | yes     |
      | e2e::PrivateWidget   | yes     |
      | e2e::PublicWidget    | yes     |
      | e2e::Widget          | yes     |
