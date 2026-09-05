Feature: Configuration precedence and provenance
  Scenario Outline: Pairwise precedence of <tiers>
    Given defaults exist at tiers "<tiers>"
    When I inspect the effective configuration twice
    Then the selected database comes from "<winner>"
    Examples:
      | tiers | winner |
      | direct,db-env | direct |
      | direct,config-file | direct |
      | direct,project | direct |
      | direct,user | direct |
      | direct,builtin | direct |
      | db-env,config-file | db-env |
      | db-env,project | db-env |
      | db-env,user | db-env |
      | db-env,builtin | db-env |
      | config-file,project | config-file |
      | config-file,user | config-file |
      | config-file,builtin | config-file |
      | project,user | project |
      | project,builtin | project |
      | user,builtin | user |

  Scenario: Every conflicting file selects one scalar tier but merges extra_args
    Given every YAML tier contains a different value
    When I inspect the effective configuration twice
    Then the selected database comes from "config-file"
    And extra_args concatenates user, then project, then config-file

  Scenario: Invalid lower-precedence files are still configuration errors
    Given defaults exist at tiers "project"
    And a malformed lower-priority file
    When I attempt configuration inspection
    Then configuration fails with "configuration in "

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
