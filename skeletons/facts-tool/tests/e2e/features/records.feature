Feature: C++ record declarations and definitions
  Structs, unions, and classes share record storage while retaining definition state.

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

  Scenario Outline: Store <qualified_name> definition state as <defined>
    Then the record definition states are
      | qualified_name   | defined   |
      | <qualified_name> | <defined> |

    Examples:
      | qualified_name       | defined |
      | e2e::CompositeWidget | yes     |
      | e2e::Deferred        | no      |
      | e2e::Payload         | yes     |
      | e2e::Policy          | yes     |
      | e2e::PrivateWidget   | yes     |
      | e2e::PublicWidget    | yes     |
      | e2e::Widget          | yes     |
