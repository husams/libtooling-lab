Feature: YAML extra_args supplement compilation database commands (B-032)
  Additional arguments preserve each translation unit's base command,
  working directory, token boundaries and independent compiler requirements.

  Scenario Outline: JSON-only and YAML-only requirements reach <family> from <representation> using <driver>
    Given an independent requirements project with "<representation>" entries and "<driver>" driver
    And retention YAML extra_args are "enabled"
    When I exercise retention for "<family>" 1 times with "no" CLI additions
    Then every run proves independent compiler effects and preserves both stored base commands
    Examples:
      | family     | representation | driver  |
      | import     | arguments      | clang++ |
      | import     | command        | clang++ |
      | import     | arguments      | g++     |
      | import     | command        | g++     |
      | extract    | arguments      | clang++ |
      | extract    | command        | clang++ |
      | extract    | arguments      | g++     |
      | extract    | command        | g++     |
      | dependency | arguments      | clang++ |
      | dependency | command        | clang++ |
      | dependency | arguments      | g++     |
      | dependency | command        | g++     |

  Scenario Outline: Both translation units keep distinct base commands under <family>
    Given an independent requirements project with "arguments" entries and "clang++" driver
    And retention YAML extra_args are "enabled"
    When I exercise retention for "<family>" 1 times with "no" CLI additions
    Then every run proves independent compiler effects and preserves both stored base commands
    Examples:
      | family     |
      | extract    |
      | dependency |

  Scenario Outline: Explicit CLI tokens supplement JSON and YAML for <family>
    Given an independent requirements project with "command" entries and "clang++" driver
    And retention YAML extra_args are "enabled"
    When I exercise retention for "<family>" 1 times with "explicit" CLI additions
    Then every run proves independent compiler effects and preserves both stored base commands
    Examples:
      | family     |
      | import     |
      | extract    |
      | dependency |

  Scenario Outline: <yaml> YAML extra_args preserves baseline behavior for <family>
    Given an independent requirements project with "arguments" entries and "clang++" driver
    And retention YAML extra_args are "<yaml>"
    When I exercise retention for "<family>" 1 times with "no" CLI additions
    Then every run proves independent compiler effects and preserves both stored base commands
    Examples:
      | family     | yaml   |
      | import     | absent |
      | import     | empty  |
      | extract    | absent |
      | extract    | empty  |
      | dependency | absent |
      | dependency | empty  |

  Scenario Outline: Repeated <family> applies YAML once and preserves the base
    Given an independent requirements project with "command" entries and "clang++" driver
    And retention YAML extra_args are "enabled"
    When I exercise retention for "<family>" 3 times with "no" CLI additions
    Then every run proves independent compiler effects and preserves both stored base commands
    Examples:
      | family     |
      | import     |
      | extract    |
      | dependency |

  Scenario: Changed YAML reaches extraction and dependency analysis without reimport
    Given an independent requirements project with "arguments" entries and "clang++" driver
    And retention YAML extra_args are "enabled"
    When I change retention YAML and run both consumers without reimport
    Then every run proves independent compiler effects and preserves both stored base commands

  Scenario: Conflicting appended macros keep compiler semantics and preserve JSON
    Given an independent requirements project with "arguments" entries and "clang++" driver
    And retention JSON defines VALUE=1 and YAML appends VALUE=2
    When I exercise retention for "extract" 1 times with "no" CLI additions
    Then every run proves independent compiler effects and preserves both stored base commands
    And VALUE=2 takes effect while VALUE=1 remains in both stored commands

  Scenario Outline: Runtime CLI additions preserve stored JSON and current YAML for <family>
    Given an independent requirements project with "command" entries and "clang++" driver
    And retention YAML extra_args are "enabled"
    When I append runtime CLI arguments to "<family>" after import
    Then every run proves independent compiler effects and preserves both stored base commands
    Examples:
      | family     |
      | extract    |
      | dependency |
