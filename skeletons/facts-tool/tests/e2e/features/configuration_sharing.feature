Feature: Transactional generated database sharing
  Scenario Outline: Database sharing under <mode>
    Given two marked projects map to one generated database
    When I initialize the shared database with "<mode>"
    Then database sharing is serialized and unrelated databases are never adopted
    Examples:
      | mode |
      | existing |
      | repeat |
      | shared-root |
      | concurrent |
      | concurrent-shared-root |
