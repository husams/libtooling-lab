Feature: Callable qualifiers remain stable and visible (B-031 AC5 through AC8)
  Background:
    Given the callable qualifier project
    When the qualifier project is extracted

  Scenario: Repeated extraction and reversed translation units preserve facts
    Then callable facts survive reruns and reversed translation units

  Scenario Outline: Existing databases migrate without losing facts
    Then callable facts survive version <version> migration and re-extraction

    Examples:
      | version |
      | 8       |
      | 9       |

  Scenario: List and detail expose the same canonical qualifiers
    Then callable list and detail render stored qualifiers and parameter defaults
