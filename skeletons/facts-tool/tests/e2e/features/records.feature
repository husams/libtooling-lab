Feature: C++ record declarations and definitions
  Structs, unions, and classes share record storage while retaining definition state.
  Integer boolean values are shown exactly as persisted by SQLite: 0 or 1.

  Background:
    Given realistic shared-header declarations, definitions, parameters, and relations
    When the real facts-tool indexes both translation units using compile_commands.json

  Scenario Outline: Store <qualified_name> as a record symbol
    Then the record symbols are
      | qualified_name   | node   |
      | <qualified_name> | <node> |

    Examples:
      | qualified_name       | node |
      | e2e::CompositeWidget | 2    |
      | e2e::Deferred        | 2    |
      | e2e::Payload         | 2    |
      | e2e::Policy          | 2    |
      | e2e::PrivateWidget   | 2    |
      | e2e::PublicWidget    | 2    |
      | e2e::Widget          | 2    |

  Scenario Outline: Store <qualified_name> is_definition as <is_definition>
    Then the persisted record symbol fields include
      | qualified_name   | is_definition   |
      | <qualified_name> | <is_definition> |

    Examples:
      | qualified_name       | is_definition |
      | e2e::CompositeWidget | 1             |
      | e2e::Deferred        | 0             |
      | e2e::Payload         | 1             |
      | e2e::Policy          | 1             |
      | e2e::PrivateWidget   | 1             |
      | e2e::PublicWidget    | 1             |
      | e2e::Widget          | 1             |
