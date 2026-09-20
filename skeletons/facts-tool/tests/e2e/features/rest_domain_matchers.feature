Feature: Native Clang DSL requests with client-controlled bindings
  Match requests accept valid AST queries without prescribing binding names.

  Scenario Outline: Valid matcher forms preserve the client's result bindings
    Given two real repositories with separate extracted fact databases
    And the repository-aware server is running
    When I submit the "<form>" Clang DSL matcher through the typed API
    Then the structured match results preserve the requested bindings and AST kinds

    Examples:
      | form          |
      | unbound       |
      | named         |
      | multiple      |
      | namespace     |
      | statement     |
      | type          |
      | internal name |

  Scenario Outline: Matching new named declarations updates global lookup
    Given two real repositories with separate extracted fact databases
    And the repository-aware server is running
    When I match a newly added "<declaration>" declaration without extracting
    Then the matched declaration is available in global symbol lookup

    Examples:
      | declaration |
      | namespace   |
      | alias       |
