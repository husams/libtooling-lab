Feature: Exclude watch sources through YAML without deleting existing facts

  Scenario Outline: Directory and glob exclusions filter events imports and extraction
    Given an indexed repository with YAML "<policy>" exclusions
    When I edit the excluded sources and then an allowed source
    Then excluded sources are neither reimported nor reindexed

    Examples:
      | policy             |
      | relative directory |
      | absolute directory |

  Scenario: Reimport skips new excluded sources in the compilation database
    Given an indexed repository with YAML "relative directory" exclusions
    When the compilation database gains an ignored and an allowed source
    Then only the allowed new source is imported and indexed
