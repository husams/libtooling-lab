Feature: Callable specifiers use semantic values (B-031 AC2, AC3, AC4)
  Background:
    Given the callable qualifier project
    When the qualifier project is extracted

  Scenario: Free and member exception specifications have positive and negative controls
    Then callable properties are
      | qualified_name             | is_noexcept |
      | qualifiers::plain          | 0 |
      | qualifiers::safe           | 1 |
      | qualifiers::safeTrue       | 1 |
      | qualifiers::unsafe         | 0 |
      | qualifiers::Specs::plain    | 0 |
      | qualifiers::Specs::safe     | 1 |
      | qualifiers::Specs::safeTrue | 1 |
      | qualifiers::Specs::unsafe   | 0 |

  Scenario: Constant evaluation and variadic specifiers are retained
    Then callable properties are
      | qualified_name              | constant_evaluation | is_variadic |
      | qualifiers::plain           | none      | 0 |
      | qualifiers::constant        | constexpr | 0 |
      | qualifiers::immediate       | consteval | 0 |
      | qualifiers::variadic        | none      | 1 |
      | qualifiers::Specs::plain     | none      | 0 |
      | qualifiers::Specs::constant  | constexpr | 0 |
      | qualifiers::Specs::immediate | consteval | 0 |
      | qualifiers::Specs::variadic  | none      | 1 |

  Scenario: Linkage and method specifiers retain positive and negative values
    Then callable properties are
      | qualified_name      | is_static | has_internal_linkage | has_extern_storage | is_inline |
      | qualifiers::plain    | 0 | 0 | 0 | 0 |
      | qualifiers::internal | 1 | 1 | 0 | 0 |
      | qualifiers::external | 0 | 0 | 1 | 0 |
      | qualifiers::constant | 0 | 0 | 0 | 1 |
      | qualifiers::(anonymous namespace)::hidden | 0 | 1 | 0 | 0 |
    And callable properties are
      | qualified_name                   | is_virtual | is_pure | is_override | is_final | is_deleted |
      | qualifiers::Specs::pure           | 1 | 1 | 0 | 0 | 0 |
      | qualifiers::Specs::overridden     | 1 | 0 | 0 | 0 | 0 |
      | qualifiers::Derived::pure         | 1 | 0 | 1 | 1 | 0 |
      | qualifiers::Derived::overridden   | 1 | 0 | 1 | 0 | 0 |
      | qualifiers::Specs::deleted        | 0 | 0 | 0 | 0 | 1 |

  Scenario: Special members and conversions retain applicable specifiers
    Then special member overloads retain their semantic properties
    And callable properties are
      | qualified_name                     | is_const | is_explicit | is_noexcept |
      | qualifiers::Special::operator bool | 1        | 1           | 1 |
      | qualifiers::Special::operator int  | 1        | 0           | 0 |

  Scenario: Templates distinguish unresolved patterns from evaluated instances
    Then callable template instances retain conditional noexcept and explicit

  Scenario: Ordinary and generic lambdas distinguish const and mutable operators
    Then callable lambda operators retain const and noexcept

  Scenario: Resolved implicit exception specifications retain throwing controls
    Then callable properties are
      | qualified_name                            | is_noexcept |
      | qualifiers::Throwing::~Throwing            | 0 |
      | qualifiers::ImplicitThrow::~ImplicitThrow  | 0 |
      | qualifiers::ImplicitSafe::~ImplicitSafe    | 1 |
      | qualifiers::FalseTrivial::~FalseTrivial    | 0 |
      | qualifiers::specialized                   | 1 |
