Feature: Configuration command policy and empty overrides
  Scenario Outline: Empty options are usage errors for <family>
    Given an isolated defaults project
    When "<family>" receives empty configuration options
    Then all empty options fail before filesystem mutation
    Examples:
      | family |
      | import |
      | extract |
      | dependency |
      | repo |
      | component |
      | dir |
      | file |
      | symbol |
      | config |

  Scenario Outline: Empty environment selector <variable>
    Given an isolated defaults project
    When configuration inspection receives empty "<variable>"
    Then configuration fails with "must not be empty"
    Examples:
      | variable |
      | FACTS_TOOL_CONF |
      | FACTS_TOOL_CONFIG |

  Scenario Outline: Write consumer <operation> initializes generated storage
    Given an isolated defaults project
    When missing storage receives "<operation>"
    Then storage has one owner and normal catalog validation ran
    Examples:
      | operation |
      | repo rm absent |
      | component rm --name absent |
      | component set-version absent v1 |
      | dir rm --path absent |
      | file rm absent.cpp |
      | file add absent.cpp --driver clang++ |

  Scenario Outline: Unused generated settings do not affect <family> with <override>
    Given an isolated defaults project
    And a compile fixture using "<override>" override and "json" commands
    When "<family>" runs with invalid unused path settings
    Then compiler effects and stored base arguments prove ordering without accumulation
    Examples:
      | family | override |
      | import | cli |
      | extract | cli |
      | dependency | cli |
      | import | env |
      | extract | env |
      | dependency | env |

  Scenario: Inspection errors retain selected-file provenance
    Given an isolated defaults project
    And a selected YAML file with "malformed"
    When I attempt configuration inspection
    Then configuration fails with "configuration in "
    And partial provenance identifies the selected file
