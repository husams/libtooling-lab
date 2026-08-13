Feature: Extracted symbol and fact inventory
  Supported declarations, definitions, parameters, and references are captured together.

  Background:
    Given realistic shared-header declarations, definitions, parameters, and relations
    When the real facts-tool indexes both translation units using compile_commands.json

  Scenario: Supported declarations use their concrete node kinds
    Then the symbol inventory includes
      | node | qualified_name         |
      | 1    | e2e::headerHelper      |
      | 1    | e2e::primitiveTypes    |
      | 1    | e2e::transform         |
      | 1    | e2e::useOne            |
      | 1    | e2e::useTwo            |
      | 1    | e2e::userDefinedTypes  |
      | 2    | e2e::CompositeWidget   |
      | 2    | e2e::Deferred          |
      | 2    | e2e::Payload           |
      | 2    | e2e::Policy            |
      | 2    | e2e::PrivateWidget     |
      | 2    | e2e::PublicWidget      |
      | 2    | e2e::Widget            |
      | 3    | e2e::Mode              |
      | 4    | e2e::Widget::value     |
      | 4    | e2e::Mode::Fast        |
      | 4    | e2e::Mode::Slow        |
      | 4    | e2e::sharedCounter     |
      | 5    | e2e                    |
      | 6    | e2e::Count             |

  Scenario: Function definitions and parameter metadata are captured
    Then the defined functions include
      | qualified_name    |
      | e2e::headerHelper |
      | e2e::transform    |
      | e2e::useOne       |
      | e2e::useTwo       |
    And the parameters for e2e::transform are
      | position | name   |
      | 0        | widget |
      | 1        | factor |
    And the parameters for e2e::headerHelper are
      | position | name  | has_default |
      | 0        | input | 0           |
      | 1        | delta | 1           |

  Scenario: Typed facts and relations retain referential integrity
    Then the facts database has no foreign-key violations
    And every relation references captured source and destination symbols
