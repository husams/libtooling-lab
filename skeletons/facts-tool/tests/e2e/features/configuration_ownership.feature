Feature: Transactional generated database ownership
  Scenario Outline: Generated ownership under <mode>
    Given two marked projects map to one generated database
    When I initialize generated ownership with "<mode>"
    Then database ownership is serialized and never adopted
    Examples:
      | mode |
      | existing |
      | repeat |
      | collision |
      | concurrent |
      | concurrent-collision |
