@variable_flow
Feature: Variable-flow overload selectors
  Variable-flow selectors can choose one overload and publish its public USR and
  type evidence.

  Background:
    Given an isolated generated overload variable-flow project

  Scenario Outline: A qualified signature selects the matching overload
    When overload variable flow is analysed for function "<function>"
    Then overload variable flow succeeds
    And the selected overload has root variable type "int" at source line <line> and parameter type "<parameter_type>"

    Examples:
      | function                     | line | parameter_type |
      | overloads::bar(bool, int)    | 3    | bool           |
      | overloads::bar(int, int)     | 7    | int            |

  Scenario: Signature whitespace is normalized
    When overload variable flow is analysed for function " overloads::bar( bool , int ) "
    Then overload variable flow succeeds
    And the selected overload has root variable type "int" at source line 3 and parameter type "bool"

  Scenario: An unqualified signature is rejected
    When overload variable flow is analysed for function "bar(bool, int)"
    Then the overload flow is rejected without publishing a run

  Scenario: A bare qualified overloaded name reports ambiguity
    When overload variable flow is analysed for function "overloads::bar"
    Then the overload flow is rejected without publishing a run
    And the overload rejection reports ambiguity for "overloads::bar"

  Scenario: A legacy qualified name and its exact USR remain usable
    When overload variable flow is analysed for function "overloads::legacy"
    Then overload variable flow succeeds
    And the overload flow captures a nonempty root function USR
    When overload variable flow is analysed with the captured legacy USR
    Then overload variable flow succeeds
    And the captured USR selects the same root variable

  Scenario: A mismatched signature is rejected without publication
    When overload variable flow is analysed for function "overloads::bar(float, int)"
    Then the overload flow is rejected without publishing a run

  Scenario Outline: Member qualifiers select the matching overload
    When overload variable flow is analysed for function "<function>"
    Then overload variable flow succeeds
    And the selected overload has root variable type "int" at source line <line> and parameter type "int"

    Examples:
      | function                              | line |
      | overloads::Worker::run(int) const     | 16   |
      | overloads::Worker::run(int) &         | 20   |

  Scenario Outline: Call operators retain their cv and ref qualifiers
    When overload variable flow is analysed for function "<function>"
    Then overload variable flow succeeds
    And the selected overload has root variable type "int" at source line <line> and parameter type "int"

    Examples:
      | function                                    | line |
      | overloads::Worker::operator()(int) const   | 24   |
      | overloads::Worker::operator()(int) &       | 28   |
