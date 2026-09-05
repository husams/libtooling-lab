Feature: Explicit path options override YAML paths (B-033)

  Scenario Outline: output aliases override facts_template for <family>
    Given an output path precedence fixture
    When I run "<family>" with "<alias>" output
    Then the explicit output path wins
    Examples:
      | family     | alias      |
      | extract    | -o         |
      | extract    | --output   |
      | dependency | -o         |
      | dependency | --output   |

  Scenario Outline: empty output values do not fall back to YAML
    Given an output path precedence fixture
    When I run "<family>" with an empty "<alias>" output
    Then the explicit empty output is rejected
    Examples:
      | family     | alias      |
      | extract    | -o         |
      | dependency | --output   |

  Scenario: a valid explicit output equal to the YAML path remains valid
    Given an output path precedence fixture
    When I run extract with --output at the fallback path
    Then the equal fallback output succeeds

  Scenario Outline: facts aliases select the requested database for symbol <action>
    Given a symbol path precedence fixture
    When I run symbol "<action>" with "<placement>" and "<alias>" facts
    Then the explicit facts path wins
    Examples:
      | action | placement | alias    |
      | list   | group     | -f       |
      | list   | group     | --facts  |
      | list   | leaf      | --facts  |
      | list   | leaf      | -f       |
      | show   | group     | -f       |
      | show   | group     | --facts  |
      | show   | leaf      | --facts  |
      | show   | leaf      | -f       |

  Scenario Outline: empty facts does not fall back to YAML for symbol <action>
    Given a symbol path precedence fixture
    When I run symbol "<action>" with an empty leaf --facts
    Then the explicit empty facts path is rejected
    Examples:
      | action |
      | list   |
      | show   |

  Scenario: browser honors an explicit missing facts path before UI startup
    Given a symbol path precedence fixture
    When I run symbol browser with a missing --facts path
    Then the explicit missing facts path is rejected

  Scenario Outline: conf aliases work at config and catalog group or leaf
    Given a catalog path precedence fixture
    When I run "<command>" with "<placement>" and "<alias>" configuration
    Then the configured catalog command succeeds
    Examples:
      | command       | placement | alias    |
      | config show   | leaf      | --conf   |
      | config show   | leaf      | -c       |
      | repo ls       | group     | --conf   |
      | repo ls       | leaf      | -c       |
      | component ls  | group     | --conf   |
      | component ls  | leaf      | -c       |
      | dir ls        | group     | --conf   |
      | dir ls        | leaf      | -c       |
      | file list     | leaf      | -c       |
      | file list     | group     | --conf   |
