Feature: Dependent initializer alignment extraction
  Dependent initializers retain their written expression without entering
  Clang's constant evaluator, while concrete initializers remain evaluated.

  Scenario: Uninstantiated dependent alignment initializers do not crash extraction
    Given a compile database for the dependent-alignment fixture
    When the real extraction command indexes the dependent-alignment fixture
    Then the dependent-alignment extraction exits successfully without incomplete diagnostics

  Scenario: Dependent expressions are persisted without evaluated values
    Given a compile database for the dependent-alignment fixture
    When the real extraction command indexes the dependent-alignment fixture
    Then the persisted variable initializers include
      | qualified_name                       | expression        | evaluated_kind | evaluated_value |
      | b020::DependentMember::value         | alignof(T)        | none           |                 |
      | b020::dependentVariable              | alignof(T)        | none           |                 |
      | b020::valueDependentVariable         | N + sizeof(T)     | none           |                 |

  Scenario: Concrete alignment expressions remain evaluated
    Given a compile database for the dependent-alignment fixture
    When the real extraction command indexes the dependent-alignment fixture
    Then the persisted variable initializers include
      | qualified_name                       | expression                             | evaluated_kind | evaluated_value |
      | b020::concreteSize                    | sizeof(AlignFixture)                   | integer        | 8               |
      | b020::concreteAlignment               | alignof(AlignFixture)                  | integer        | 8               |
      | b020::instantiatedMember              | DependentMember<AlignFixture>::value   | integer        | 8               |
    And the instantiated dependent-member value symbol is present

  Scenario: Dependent alignment extraction is deterministic and referentially intact
    Given a compile database for the dependent-alignment fixture
    When the real extraction command indexes the dependent-alignment fixture twice
    Then the dependent-alignment extraction exits successfully without incomplete diagnostics
    And both runs produce identical symbol identities
    And the output database passes PRAGMA foreign_key_check
