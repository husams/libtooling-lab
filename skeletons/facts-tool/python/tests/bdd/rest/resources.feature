Feature: Installed SDK identifies symbols and source files without database paths
  Scenario Outline: Search and analyze a file through the resource SDK
    Given an indexed project and a <kind> resource SDK client
    When I search for a fully qualified function name
    Then the SDK returns its typed repository and defining file identity
    When I match that file with a Clang DSL expression
    Then the SDK receives a structured completed operation result

    Examples:
      | kind  |
      | sync  |
      | async |
