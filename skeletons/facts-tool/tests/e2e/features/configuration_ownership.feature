Feature: Transactional generated database sharing
  Scenario Outline: Generated ownership under <mode>
    Given two marked projects map to one generated database
    When I initialize generated ownership with "<mode>"
    Then database sharing is serialized and unrelated databases are never adopted
    Examples:
      | mode |
      | existing |
      | repeat |
      | shared-root |
      | concurrent |
      | concurrent-shared-root |
