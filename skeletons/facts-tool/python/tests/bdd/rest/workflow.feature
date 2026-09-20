Feature: Python REST clients execute the native analysis workflow
  Background:
    Given a native facts-tool REST server

  Scenario Outline: Analyse C++ with either Python REST client
    Given an authenticated <kind> SDK client
    And a C++ project with an answer function
    When I import and extract the project through the SDK
    Then both remote commands succeed with captured process results
    And the SQLite SDK finds the extracted answer function
    When I invoke the named call graph command and poll its job
    Then the completed job is discoverable with metadata and detailed output

    Examples:
      | kind  |
      | sync  |
      | async |

  Scenario Outline: Discover the server's capabilities
    Given an authenticated <kind> SDK client
    When I request the server health and API capabilities
    Then the server advertises CLI commands job routes and watch state

    Examples:
      | kind  |
      | sync  |
      | async |
