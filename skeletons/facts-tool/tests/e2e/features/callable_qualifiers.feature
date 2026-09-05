Feature: Callable qualifiers survive extraction (B-031 AC1, AC5, AC8)
  Background:
    Given the callable qualifier project
    When the qualifier project is extracted

  Scenario Outline: Every legal cv and ref qualifier combination stays distinct
    Then callable "qualifiers::Cv::<name>" has exactly one overload with <const>, <volatile>, <ref>

    Examples:
      | name  | const | volatile | ref    |
      | plain | 0 | 0 | none |
      | plain | 1 | 0 | none |
      | plain | 0 | 1 | none |
      | plain | 1 | 1 | none |
      | ref | 0 | 0 | lvalue |
      | ref | 1 | 0 | lvalue |
      | ref | 0 | 1 | lvalue |
      | ref | 1 | 1 | lvalue |
      | ref | 0 | 0 | rvalue |
      | ref | 1 | 0 | rvalue |
      | ref | 0 | 1 | rvalue |
      | ref | 1 | 1 | rvalue |

  Scenario: Parameter and return qualifiers do not become callable qualifiers
    Then callable properties are
      | qualified_name               | is_const | is_volatile | ref_qualifier | is_noexcept | is_static |
      | qualifiers::Cv::staticControl | 0        | 0           | none          | 0           | 1         |
      | qualifiers::freeControl       | 0        | 0           | none          | 0           | 0         |
