Feature: Configuration precedence and provenance
  Scenario Outline: Pairwise precedence of <tiers>
    Given defaults exist at tiers "<tiers>"
    When I inspect the effective configuration twice
    Then the selected database comes from "<winner>"
    Examples:
      | tiers | winner |
      | direct,db-env | direct |
      | direct,cli | direct |
      | direct,env | direct |
      | direct,project | direct |
      | direct,xdg | direct |
      | direct,home | direct |
      | direct,builtin | direct |
      | db-env,cli | db-env |
      | db-env,env | db-env |
      | db-env,project | db-env |
      | db-env,xdg | db-env |
      | db-env,home | db-env |
      | db-env,builtin | db-env |
      | cli,env | cli |
      | cli,project | cli |
      | cli,xdg | cli |
      | cli,home | cli |
      | cli,builtin | cli |
      | env,project | env |
      | env,xdg | env |
      | env,home | env |
      | env,builtin | env |
      | project,xdg | project |
      | project,home | project |
      | project,builtin | project |
      | xdg,home | xdg |
      | xdg,builtin | xdg |
      | home,builtin | home |

  Scenario: Every conflicting file selects one compiler list
    Given every YAML tier contains a different value
    When I inspect the effective configuration twice
    Then the selected database comes from "cli"
    And only the highest YAML compiler list is used

  Scenario: Invalid lower files are not opened
    Given defaults exist at tiers "project"
    And a malformed lower-priority file
    When I inspect the effective configuration twice
    Then the selected database comes from "project"

  Scenario Outline: Missing keys use built-ins
    Given the selected YAML contains "<content>"
    When I inspect the effective configuration twice
    Then all absent settings retain built-in provenance
    Examples:
      | content |
      | empty |
      | {} |

  Scenario: Selected directory reports unreadability
    Given an unreadable selected YAML directory
    When I attempt configuration inspection
    Then configuration fails with "configuration in "

  Scenario: Missing explicit selector does not fall back
    Given defaults exist at tiers "project"
    And an explicit missing configuration selector
    When I attempt configuration inspection
    Then configuration fails with "file not found"
    And search diagnostics include setting path outcomes and remedy
